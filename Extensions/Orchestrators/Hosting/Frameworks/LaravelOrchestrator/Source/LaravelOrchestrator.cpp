#include "LaravelOrchestrator.h"

#include "Core/NovaLog.h"
#include "Core/ProgressTracker.h"
#include "Core/ExtensionRegistry.h"
#include "ExtensionSpecific/IContentForge.h"
#include "ExtensionSpecific/ICanvasRuntimeSurfaceProvider.h"
#include "KeyForgeDeploymentContracts.h"
#include "TerminalAgent.h"
#include "DockerOrchestrator.h"
#include "IComposerAgent.h"

#include <filesystem>
#include <fstream>
#include <json.hpp>
#include <sstream>
#include <cstdlib>

#if defined(__linux__)
#include <unistd.h>
#endif

namespace {

void PublishDeploymentToast(const std::string& title,
                            const std::string& message,
                            Core::CanvasNotificationSeverity severity) {
    auto* canvas = dynamic_cast<Core::ICanvasRuntimeSurfaceProvider*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("canvascore"));
    if (!canvas) {
        return;
    }

    Core::CanvasToastNotification toast;
    toast.SourceExtensionId = "laravelorchestrator";
    toast.Title = title;
    toast.Message = message;
    toast.Severity = severity;
    canvas->PublishCanvasToast(toast);
}

bool ResolveRequestedLaravelContent(const Core::CanvasMenuActionRequest& request,
                                   Core::IContentForge* contentForge,
                                   Core::LocalContentDescriptor& outContent,
                                   std::string& outError) {
    const auto contentId = request.ContextValues.find("contentId");
    if (contentId == request.ContextValues.end() || contentId->second.empty()) {
        outError = "Select a ContentForge content pack before running a lifecycle action.";
        return false;
    }
    if (!contentForge->ResolveLocalContent(contentId->second, outContent)) {
        outError = "The selected content pack is not registered by ContentForge.";
        return false;
    }
    if (outContent.framework != "laravel" || outContent.orchestrator != "laravelorchestrator") {
        outError = "The selected content pack is not owned by LaravelOrchestrator.";
        return false;
    }
    return true;
}

bool ResolveLaravelContent(const std::string& contentId,
                          Core::IContentForge* contentForge,
                          Core::LocalContentDescriptor& outContent,
                          std::string& outError) {
    if (contentId.empty()) {
        outError = "A ContentForge content pack ID is required.";
        return false;
    }
    if (!contentForge || !contentForge->ResolveLocalContent(contentId, outContent)) {
        outError = "The requested local content is not registered by ContentForge.";
        return false;
    }
    if (outContent.framework != "laravel" || outContent.orchestrator != "laravelorchestrator") {
        outError = "The requested content is not a LaravelOrchestrator content pack.";
        return false;
    }
    return true;
}

bool ValidateLocalLaravelProject(const Core::LocalContentDescriptor& content, std::string& outError) {
    const std::filesystem::path projectPath(content.path);
    if (!std::filesystem::is_directory(projectPath)) {
        outError = "The registered content path does not exist.";
        return false;
    }
    if (!std::filesystem::is_regular_file(projectPath / "artisan") ||
        !std::filesystem::is_regular_file(projectPath / "composer.json") ||
        !std::filesystem::is_regular_file(projectPath / content.composeFile)) {
        outError = "Local content is not a supported Laravel Compose project.";
        return false;
    }
    return true;
}

bool MaterializeLaravelRelease(Core::IContentForge* contentForge,
                               const Core::LocalContentDescriptor& content,
                               Core::LocalContentRelease& outRelease,
                               Core::LocalContentDescriptor& outReleaseContent,
                               std::string& outError) {
    if (!contentForge->MaterializeLocalContent(content.id, "", outRelease)) {
        outError = "ContentForge could not materialize a local release for the selected content.";
        return false;
    }
    outReleaseContent = content;
    outReleaseContent.path = outRelease.releasePath;
    auto* composer = dynamic_cast<Core::IComposerAgent*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("composeragent"));
    // Sail Compose projects build from vendor/laravel/sail/runtimes.  Sail is
    // conventionally declared in require-dev, so excluding dev packages would
    // make the declared Compose build context disappear.
    const bool needsSailRuntime = std::filesystem::is_regular_file(
        std::filesystem::path(outRelease.releasePath) / "compose.yaml");
    if (!composer || !composer->IsComposerInstalled() ||
        !composer->InstallDependenciesSync(outRelease.releasePath, !needsSailRuntime)) {
        outError = "ComposerAgent could not materialize production dependencies for the release.";
        return false;
    }
    if (!ValidateLocalLaravelProject(outReleaseContent, outError)) {
        return false;
    }
    return true;
}

bool InjectLocalDevelopmentEnvironment(const Core::LocalContentDescriptor& content,
                                       const Core::LocalContentRelease& release,
                                       std::string& outError) {
    if (content.localEnvironmentFile.empty()) {
        outError = "This local content pack has no localDevelopment.environmentFile bridge.";
        return false;
    }
    std::error_code error;
    const auto source = std::filesystem::weakly_canonical(content.localEnvironmentFile, error);
    if (error || !std::filesystem::is_regular_file(source)) {
        outError = "The configured local development environment file is unavailable.";
        return false;
    }
    const auto destination = std::filesystem::path(release.releasePath) / ".env";
    std::filesystem::copy_file(source, destination, std::filesystem::copy_options::none, error);
    if (error) {
        outError = "The local development environment could not be injected into the release.";
        return false;
    }
    NOVA_LOG(("[LaravelOrchestrator] Injected local development environment into release '" + release.releaseId + "'.").c_str(), LogType::Log);
    return true;
}

bool InjectRuntimeEnvironment(const Core::LocalContentDescriptor& content,
                              const Core::LocalContentRelease& release,
                              std::string& outError) {
    if (content.sourceType == "local-path" && !content.localEnvironmentFile.empty()) {
        return InjectLocalDevelopmentEnvironment(content, release, outError);
    }
    try {
        std::ifstream contentFile(content.manifestPath);
        nlohmann::json manifest; contentFile >> manifest;
        const auto environment = manifest.value("runtimeEnvironment", nlohmann::json::object());
        auto* keyForge = dynamic_cast<KeyForge::IDeploymentSecretBroker*>(
            Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("keyforge"));
        if (!keyForge || environment.empty()) { outError = "Production content requires a KeyForge runtimeEnvironment contract."; return false; }
        KeyForge::RuntimeEnvironmentRequest request;
        request.requestorExtensionId = "laravelorchestrator";
        request.targetId = environment.value("targetId", "");
        request.remoteReleasePath = release.releasePath;
        request.publicValues = environment.value("publicValues", std::map<std::string, std::string>{});
        request.secretReferences = environment.value("secretReferences", std::map<std::string, std::string>{});
        // A daemon calls its co-located Auth API over loopback, while a human
        // device-login browser may need the VM's host-only address. Keep that
        // public URL as explicit runtime configuration, never pack content.
        if (const auto* publicAuthBase = std::getenv("CELESTIA_AUTH_API_PUBLIC_BASE_URL"); publicAuthBase) {
            const std::string publicUrl(publicAuthBase);
            if ((publicUrl.rfind("http://", 0) == 0 || publicUrl.rfind("https://", 0) == 0) &&
                publicUrl.find_first_of(" \t\r\n\"'`|&;<>") == std::string::npos) {
                request.publicValues["APP_URL"] = publicUrl;
            }
        }
#if defined(__linux__)
        // Sail's bind-mounted PHP worker must use the same host identity as
        // the systemd service that owns the materialized release.  Do not
        // persist a machine-specific UID/GID in the declarative content pack.
        request.publicValues["WWWUSER"] = std::to_string(getuid());
        request.publicValues["WWWGROUP"] = std::to_string(getgid());
#endif
        const auto receipt = keyForge->MaterializeRemoteRuntimeEnvironment(request);
        if (!receipt.accepted || !std::filesystem::is_regular_file(std::filesystem::path(release.releasePath) / ".env")) {
            outError = "KeyForge runtime environment materialization failed: " + receipt.receipt; return false;
        }
        return true;
    } catch (...) { outError = "The runtimeEnvironment contract is invalid."; return false; }
}

bool IsConfigurationDepth(const std::string& value) {
    return value == "auto" || value == "minimal" || value == "normal" || value == "advanced";
}

std::filesystem::path RuntimeStateRoot() {
    // The installed package tree is read-only.  Service-mode state must live
    // beside the ContentForge runtime releases, not below /opt/celestianova.
    if (const auto* runtimeRoot = std::getenv("CELESTIA_RUNTIME_ROOT"); runtimeRoot && *runtimeRoot) {
        return std::filesystem::path(runtimeRoot) / ".runtime";
    }
    return std::filesystem::current_path() / "Content" / ".runtime";
}

struct RemoteTarget {
    std::string id;
    std::string host;
    unsigned short port = 22;
    std::string user;
    std::string knownHostsFile;
    std::string releaseRoot;
    std::string credentialReference;
};

struct NovaIdLoginContract {
    std::string authorizationServerId;
    std::string applicationId;
    std::vector<std::string> scopes;
    std::vector<std::string> audiences;
};

bool IsSafeHttpUrl(const std::string& value) {
    if (value.rfind("http://", 0) != 0 && value.rfind("https://", 0) != 0) {
        return false;
    }
    return value.find_first_of(" \t\r\n\"'`|&;<>") == std::string::npos;
}

bool LoadNovaIdLoginContract(const Core::LocalContentDescriptor& content,
                             NovaIdLoginContract& outContract,
                             bool& outRequired,
                             std::string& outError) {
    try {
        std::ifstream contentFile(content.manifestPath);
        nlohmann::json contentManifest;
        contentFile >> contentManifest;
        outRequired = contentManifest.value("remoteDeployment", nlohmann::json::object()).value("requiresNovaId", false);
        const auto deviceFlow = contentManifest.value("novaIdDeviceFlow", nlohmann::json::object());
        if (deviceFlow.empty()) {
            if (!outRequired) {
                return true;
            }
            outError = "This remote content pack requires Nova ID but has no OAuth device-flow contract.";
            return false;
        }
        const auto authorizeUrl = deviceFlow.value("deviceAuthorizeUrl", "");
        const auto approveUrl = deviceFlow.value("deviceApproveUrl", "");
        const auto tokenUrl = deviceFlow.value("deviceTokenUrl", "");
        if (!IsSafeHttpUrl(authorizeUrl) || !IsSafeHttpUrl(approveUrl) || !IsSafeHttpUrl(tokenUrl)) {
            outError = "The OAuth device-flow endpoint declaration is unsafe or incomplete.";
            return false;
        }
        const auto application = deviceFlow.value("oauthApplication", nlohmann::json::object());
        outContract.authorizationServerId = application.value("authorizationServerId", "");
        outContract.applicationId = application.value("applicationId", "");
        outContract.scopes = application.value("scopes", std::vector<std::string>{});
        outContract.audiences = application.value("audiences", std::vector<std::string>{});
        if (outContract.authorizationServerId.empty() || outContract.applicationId.empty() || outContract.scopes.empty()) {
            outError = "The OAuth device-flow application declaration is incomplete.";
            return false;
        }
        return true;
    } catch (const std::exception&) {
        outError = "The selected content pack's Nova ID contract is invalid JSON.";
        return false;
    }
}

bool IsSafeRemoteDirectory(const std::string& value) {
    if (value.empty() || value.front() != '/' || value.find("..") != std::string::npos) {
        return false;
    }
    return value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789/_-.") == std::string::npos;
}

bool LoadRemoteTarget(const Core::LocalContentDescriptor& content, RemoteTarget& outTarget, std::string& outError) {
    try {
        std::ifstream contentFile(content.manifestPath);
        nlohmann::json contentManifest;
        contentFile >> contentManifest;
        const auto remoteDeployment = contentManifest.value("remoteDeployment", nlohmann::json::object());
        const auto targetPathValue = remoteDeployment.value("targetManifest", "");
        if (targetPathValue.empty()) {
            outError = "The selected content pack has no remoteDeployment.targetManifest.";
            return false;
        }
        const auto targetPath = std::filesystem::path(content.manifestPath).parent_path() / targetPathValue;
        std::ifstream targetFile(targetPath);
        nlohmann::json targetManifest;
        targetFile >> targetManifest;
        outTarget.id = targetManifest.value("id", "");
        outTarget.host = targetManifest.value("host", "");
        outTarget.port = static_cast<unsigned short>(targetManifest.value("port", 22));
        outTarget.user = targetManifest.value("user", "");
        outTarget.releaseRoot = remoteDeployment.value("releaseRoot", targetManifest.value("releaseRoot", ""));
        outTarget.credentialReference = targetManifest.value("sshAgentCredentialRef", "");
        const auto knownHostsRelative = targetManifest.value("knownHostsFile", "");
        outTarget.knownHostsFile = (std::filesystem::current_path() / knownHostsRelative).string();
        if (outTarget.id.empty() || outTarget.host.empty() || outTarget.user.empty() || outTarget.port == 0 ||
            outTarget.credentialReference.rfind("keyforge://", 0) != 0 || !IsSafeRemoteDirectory(outTarget.releaseRoot)) {
            outError = "The remote target manifest is incomplete or unsafe.";
            return false;
        }
        if (!std::filesystem::is_regular_file(outTarget.knownHostsFile)) {
            outError = "The remote target known-hosts file is missing; capture and verify the VM host key before connecting.";
            return false;
        }
        return true;
    } catch (const std::exception&) {
        outError = "The remote deployment or target manifest is invalid JSON.";
        return false;
    }
}

CoreTerminal::ITerminalAgent* ResolveTerminalAgent() {
    return dynamic_cast<CoreTerminal::ITerminalAgent*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
}

bool IsSafeLocalComposePath(const std::string& value) {
    if (value.empty() || value.find("..") != std::string::npos) {
        return false;
    }
    return value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789/_-.\\:") == std::string::npos;
}

bool RepairSailWritablePaths(const Core::LocalContentRelease& release,
                             const Core::LocalContentDescriptor& content,
                             std::string& outError) {
    // Auth API's current local hosting profile is Sail. Its bind mount may
    // contain root-owned files created during release materialization, while
    // the PHP server runs as `sail`. Keep this repair fixed and narrow; it is
    // not a general-purpose post-deploy command channel.
    if (content.primaryService != "laravel.test") {
        return true;
    }
    const auto composePath = (std::filesystem::path(release.releasePath) / content.composeFile).string();
    if (!IsSafeLocalComposePath(composePath)) {
        outError = "Laravel post-deploy repair rejected an unsafe Compose path.";
        return false;
    }
    auto* terminal = ResolveTerminalAgent();
    if (!terminal) {
        outError = "Laravel post-deploy repair requires TerminalAgent.";
        return false;
    }
    CoreTerminal::TerminalCommandRequest request;
    request.workingDirectory = release.releasePath;
    request.command = "docker compose -f \"" + composePath +
        "\" exec -T --user root laravel.test sh -c \"mkdir -p storage/logs bootstrap/cache && chown -R sail:sail storage bootstrap/cache && chmod -R ug+rwX storage bootstrap/cache\"";
    const auto result = terminal->ExecuteCommandSync(request);
    if (result.exitCode != 0) {
        outError = "Laravel post-deploy writable-path repair failed.";
        return false;
    }
    return true;
}

bool BootstrapAuthApiLocalDatabase(const Core::LocalContentRelease& release,
                                   const Core::LocalContentDescriptor& content,
                                   std::string& outError) {
    if (content.id != "auth-api" || content.primaryService != "laravel.test") {
        return true;
    }
    const auto composePath = (std::filesystem::path(release.releasePath) / content.composeFile).string();
    if (!IsSafeLocalComposePath(composePath)) {
        outError = "Auth API post-deploy bootstrap rejected an unsafe Compose path.";
        return false;
    }
    auto* terminal = ResolveTerminalAgent();
    if (!terminal) {
        outError = "Auth API post-deploy bootstrap requires TerminalAgent.";
        return false;
    }
    const std::string composePrefix = "docker compose -f \"" + composePath + "\" exec -T laravel.test php artisan ";
    // `up -d` returns once containers have been started, not once MariaDB has
    // accepted connections. A bounded retry makes first-boot deployments
    // reliable while keeping all commands fixed and content-specific.
    CoreTerminal::TerminalCommandRequest request;
    request.workingDirectory = release.releasePath;
    request.command = "attempt=1; while [ $attempt -le 30 ]; do " +
        composePrefix + "migrate --force --database=AuthConnection --path=database/migrations/AuthConnection && " +
        composePrefix + "migrate --force --database=RestrictionsConnection --path=database/migrations/RestrictionsConnection && " +
        composePrefix + "nova:local-admin --name=Admin && exit 0; " +
        "attempt=$((attempt + 1)); sleep 2; done; exit 1";
    const auto result = terminal->ExecuteCommandSync(request);
    if (result.exitCode != 0) {
        outError = "Auth API post-deploy database bootstrap did not become ready in time.";
        return false;
    }
    return true;
}

std::string DescribeContent(const Core::LocalContentDescriptor& content) {
    std::ostringstream message;
    message << "Content '" << content.id << "' is valid for Laravel local hosting"
            << " (path: " << content.path
            << ", compose: " << content.composeFile
            << ", service: " << content.primaryService;
    if (!content.healthEndpoint.empty()) {
        message << ", health: " << content.healthEndpoint;
    }
    message << ").";
    return message.str();
}

} // namespace

LaravelOrchestratorModule::LaravelOrchestratorModule() {}
LaravelOrchestratorModule::~LaravelOrchestratorModule() {}

std::string LaravelOrchestratorModule::GetActiveReleasePath(const std::string& contentId) const {
    {
        std::lock_guard<std::mutex> lock(ActiveReleaseMutex_);
        const auto found = ActiveReleasePaths_.find(contentId);
        if (found != ActiveReleasePaths_.end()) {
            return found->second;
        }
    }

    const auto statePath = RuntimeStateRoot() / contentId / "active-release.json";
    std::ifstream stateFile(statePath);
    if (!stateFile) {
        return "";
    }
    try {
        nlohmann::json state;
        stateFile >> state;
        const auto releasePath = state.value("releasePath", "");
        if (!releasePath.empty() && std::filesystem::is_directory(releasePath)) {
            return releasePath;
        }
    } catch (...) {
        NOVA_LOG(("[LaravelOrchestrator] Ignoring invalid release state: " + statePath.string()).c_str(), LogType::Warning);
    }
    return "";
}

void LaravelOrchestratorModule::RememberActiveRelease(const std::string& contentId, const std::string& releasePath) {
    {
        std::lock_guard<std::mutex> lock(ActiveReleaseMutex_);
        ActiveReleasePaths_[contentId] = releasePath;
    }
    const auto stateDirectory = RuntimeStateRoot() / contentId;
    std::error_code error;
    std::filesystem::create_directories(stateDirectory, error);
    if (error) {
        NOVA_LOG(("[LaravelOrchestrator] Failed to create release state directory: " + stateDirectory.string()).c_str(), LogType::Warning);
        return;
    }
    std::ofstream stateFile(stateDirectory / "active-release.json", std::ios::trunc);
    if (!stateFile) {
        NOVA_LOG("[LaravelOrchestrator] Failed to persist active release state.", LogType::Warning);
        return;
    }
    stateFile << nlohmann::json{{"contentId", contentId}, {"releasePath", releasePath}}.dump(2) << '\n';
}

bool LaravelOrchestratorModule::BeginNovaIdLogin(const Core::LocalContentDescriptor& content,
                                                  std::string& outLoginUrl,
                                                  std::string& outError) {
    NovaIdLoginContract contract;
    bool required = false;
    if (!LoadNovaIdLoginContract(content, contract, required, outError)) {
        return false;
    }
    if (contract.applicationId.empty()) {
        outError = "The selected content pack does not require a Nova ID device login.";
        return false;
    }
    auto* keyForge = dynamic_cast<KeyForge::IDeploymentSecretBroker*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("keyforge"));
    if (!keyForge) {
        outError = "Nova ID device login requires the KeyForge credential broker.";
        return false;
    }
    KeyForge::DeviceAuthorizationRequest request;
    request.requestorExtensionId = "laravelorchestrator";
    request.applicationId = contract.applicationId;
    request.authorizationServerId = contract.authorizationServerId;
    request.scopes = contract.scopes;
    request.audiences = contract.audiences;
    const auto response = keyForge->BeginDeviceAuthorization(request);
    if (!response.accepted) {
        outError = "Nova ID device login unavailable: " + response.receipt;
        return false;
    }
    if (!IsSafeHttpUrl(response.verificationUri) || response.deviceCode.empty()) {
        outError = "KeyForge returned an unsafe or incomplete device authorization response.";
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(NovaIdSessionMutex_);
        NovaIdSessions_[content.id] = {response.deviceCode, response.verificationUri, "Login Required - approve device code", ""};
    }
    outLoginUrl = response.verificationUri;
    return true;
}

bool LaravelOrchestratorModule::PollNovaIdLogin(const Core::LocalContentDescriptor& content,
                                                 std::string& outStatus,
                                                 std::string& outError) {
    NovaIdLoginContract contract;
    bool required = false;
    if (!LoadNovaIdLoginContract(content, contract, required, outError)) {
        return false;
    }
    auto* keyForge = dynamic_cast<KeyForge::IDeploymentSecretBroker*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("keyforge"));
    if (!keyForge) {
        outError = "Nova ID device-token exchange requires KeyForge.";
        return false;
    }
    std::string deviceCode;
    {
        std::lock_guard<std::mutex> lock(NovaIdSessionMutex_);
        const auto session = NovaIdSessions_.find(content.id);
        if (session == NovaIdSessions_.end() || session->second.sessionId.empty()) {
            outError = "Begin Nova ID login before polling its device authorization.";
            return false;
        }
        deviceCode = session->second.sessionId;
    }
    const auto response = keyForge->PollDeviceAuthorization({"laravelorchestrator", contract.applicationId,
                                                              contract.authorizationServerId, deviceCode});
    if (response.pending) {
        outStatus = "Waiting for approval";
        return true;
    }
    if (!response.accepted || response.accessToken.empty()) {
        outError = "Nova ID device-token exchange failed: " + response.receipt;
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(NovaIdSessionMutex_);
        auto& session = NovaIdSessions_[content.id];
        session.accessToken = response.accessToken;
        session.status = "Authenticated";
    }
    outStatus = "Authenticated";
    return true;
}

void LaravelOrchestratorModule::LogoutNovaId(const std::string& contentId) {
    std::lock_guard<std::mutex> lock(NovaIdSessionMutex_);
    NovaIdSessions_.erase(contentId);
}

bool LaravelOrchestratorModule::HasNovaIdToken(const std::string& contentId) const {
    std::lock_guard<std::mutex> lock(NovaIdSessionMutex_);
    const auto found = NovaIdSessions_.find(contentId);
    return found != NovaIdSessions_.end() && !found->second.accessToken.empty();
}

bool LaravelOrchestratorModule::HasAuthenticatedNovaIdSession() const {
    std::lock_guard<std::mutex> lock(NovaIdSessionMutex_);
    for (const auto& [contentId, session] : NovaIdSessions_) {
        (void)contentId;
        if (!session.accessToken.empty()) {
            return true;
        }
    }
    return false;
}

bool LaravelOrchestratorModule::AuthorizeRemoteControlDispatch(
    const std::string& targetId,
    const std::string& requiredCapability,
    Core::RemoteControlDispatchAuthorization& outAuthorization,
    std::string& outError) const {
    outAuthorization.authorizationHeader.clear();
    if (targetId.empty() || requiredCapability != "mesh.remote.execute") {
        outError = "Remote dispatch requires a declared target and the mesh.remote.execute capability.";
        return false;
    }
    std::lock_guard<std::mutex> lock(NovaIdSessionMutex_);
    for (const auto& [contentId, session] : NovaIdSessions_) {
        (void)contentId;
        if (!session.accessToken.empty()) {
            // This is an in-process, one-request authorization bridge.  The
            // caller must keep it transient and neither configuration nor UI
            // receives the bearer value.
            outAuthorization.authorizationHeader = "Bearer " + session.accessToken;
            return true;
        }
    }
    outError = "No approved in-memory Nova ID session is available.";
    return false;
}

void LaravelOrchestratorModule::StartupModule() {
    NOVA_LOG("[LaravelOrchestrator] StartupModule called", LogType::Log);
}

void LaravelOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[LaravelOrchestrator] ShutdownModule called", LogType::Log);
    std::vector<std::thread> workers;
    {
        std::lock_guard<std::mutex> lock(DeploymentMutex_);
        workers.swap(DeploymentWorkers_);
    }
    for (auto& worker : workers) {
        if (worker.joinable()) worker.join();
    }
}

bool LaravelOrchestratorModule::QueueLocalDeployment(const std::string& contentId,
                                                      const std::string& profile,
                                                      std::string& outJobId) {
    {
        std::lock_guard<std::mutex> lock(DeploymentMutex_);
        if (!QueuedDeployments_.insert(contentId).second) {
            outJobId = "deploy:" + contentId;
            return false;
        }
        outJobId = "deploy:" + contentId;
        Core::ProgressTracker::Publish({outJobId, "laravelorchestrator", "Queued for background deployment", 5, true});
        DeploymentWorkers_.emplace_back([this, contentId, profile, outJobId] {
            const auto deployment = DeployLocalContent(contentId, profile);
            PublishDeploymentToast(deployment.succeeded ? "CONTENT_STARTED" : "CONTENT_START_FAILED",
                                  deployment.message.empty() ? "Deployment completed." : deployment.message,
                                  deployment.succeeded ? Core::CanvasNotificationSeverity::Success : Core::CanvasNotificationSeverity::Error);
            std::lock_guard<std::mutex> lock(DeploymentMutex_);
            QueuedDeployments_.erase(contentId);
        });
    }
    return true;
}

std::vector<Core::FExtensionCliArgDescriptor> LaravelOrchestratorModule::GetCliArgDescriptors() const {
    return {
        {"deploy-local-content", "Start a registered local Laravel content pack.", true},
        {"stop-local-content", "Stop a registered local Laravel content pack.", true},
        {"local-content-status", "Report whether a registered Laravel content pack's primary Compose service is running.", true},
        {"local-content-dry-run", "Validate a registered Laravel content pack without starting Docker.", true},
        {"local-content-logs", "Report the log surface for a registered Laravel content pack (Docker log streaming is not exposed yet).", true},
        {"remote-bootstrap", "Queue verified Docker prerequisites on the content pack's configured SSH target.", true},
        {"deploy-remote-content", "Stage a registered Laravel content release on its configured SSH target.", true},
        {"remote-content-status", "Query the configured remote Compose release status over SSH.", true},
        {"configuration-depth", "Set deploy configuration depth: auto, minimal, normal, or advanced.", true}
    };
}

void LaravelOrchestratorModule::ApplyCliArgs(const std::vector<Core::FExtensionCliArg>& args) {
    std::string configurationDepth = "auto";
    for (const auto& argument : args) {
        if (argument.Flag == "configuration-depth" && !argument.Value.empty()) {
            configurationDepth = argument.Value;
        }
    }
    if (!IsConfigurationDepth(configurationDepth)) {
        NOVA_LOG("[LaravelOrchestrator] --configuration-depth must be auto, minimal, normal, or advanced.", LogType::Error);
        return;
    }

    for (const auto& argument : args) {
        if (argument.Value.empty() || argument.Flag == "configuration-depth") {
            continue;
        }

        if (argument.Flag == "deploy-local-content") {
            const auto deployment = DeployLocalContent(argument.Value, configurationDepth);
            const auto message = "[LaravelOrchestrator] " + deployment.message;
            NOVA_LOG(message.c_str(), deployment.succeeded ? LogType::Log : LogType::Error);
            continue;
        }

        if (argument.Flag == "stop-local-content") {
            auto* docker = dynamic_cast<IDockerOrchestrator*>(
                Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("dockerorchestrator"));
            const auto releasePath = GetActiveReleasePath(argument.Value);
            const auto stopped = docker && !releasePath.empty() ? docker->StopCompose(releasePath) : DockerComposeResult{};
            NOVA_LOG(("[LaravelOrchestrator] " + std::string(stopped.succeeded ? "Stopped " : "Stop failed for ") + argument.Value + (stopped.output.empty() ? "" : ": " + stopped.output)).c_str(),
                     stopped.succeeded ? LogType::Log : LogType::Error);
            continue;
        }

        if (argument.Flag == "deploy-remote-content") {
            const auto deployment = DeployRemoteContent(argument.Value, configurationDepth);
            const auto message = "[LaravelOrchestrator] " + deployment.message;
            NOVA_LOG(message.c_str(), deployment.succeeded ? LogType::Log : LogType::Error);
            continue;
        }

        auto* contentForge = dynamic_cast<Core::IContentForge*>(
            Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("contentforge"));
        Core::LocalContentDescriptor content;
        std::string error;
        if (!ResolveLaravelContent(argument.Value, contentForge, content, error)) {
            NOVA_LOG(("[LaravelOrchestrator] " + error).c_str(), LogType::Error);
            continue;
        }

        if (argument.Flag == "local-content-dry-run") {
            Core::LocalContentRelease release;
            Core::LocalContentDescriptor releaseContent;
            if (!MaterializeLaravelRelease(contentForge, content, release, releaseContent, error)) {
                NOVA_LOG(("[LaravelOrchestrator] Dry run materialization failed: " + error).c_str(), LogType::Error);
                continue;
            }
            auto* docker = dynamic_cast<IDockerOrchestrator*>(
                Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("dockerorchestrator"));
            const auto validation = docker ? docker->ValidateCompose(release.releasePath, content.composeFile) : DockerComposeResult{};
            if (!docker || !validation.succeeded) {
                NOVA_LOG(("[LaravelOrchestrator] Dry run compose validation failed: " + (docker ? validation.output : "DockerOrchestrator is not loaded.")).c_str(), LogType::Error);
                continue;
            }
            NOVA_LOG(("[LaravelOrchestrator] Dry run succeeded for release '" + release.releaseId + "'. " + DescribeContent(releaseContent)).c_str(), LogType::Log);
        } else if (argument.Flag == "local-content-status") {
            auto* docker = dynamic_cast<IDockerOrchestrator*>(
                Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("dockerorchestrator"));
            if (!docker || content.primaryService.empty()) {
                NOVA_LOG("[LaravelOrchestrator] DockerOrchestrator or deployment.primaryService is unavailable.", LogType::Error);
                continue;
            }
            const auto releasePath = GetActiveReleasePath(content.id);
            if (releasePath.empty()) {
                NOVA_LOG("[LaravelOrchestrator] No active materialized release is tracked for this process.", LogType::Warning);
                continue;
            }
            const bool running = docker->IsComposeServiceRunning(releasePath, content.primaryService, content.composeFile);
            NOVA_LOG(("[LaravelOrchestrator] Content '" + content.id + "' is " + (running ? "running." : "stopped.")).c_str(), LogType::Log);
        } else if (argument.Flag == "local-content-logs") {
            auto* docker = dynamic_cast<IDockerOrchestrator*>(
                Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("dockerorchestrator"));
            const auto releasePath = GetActiveReleasePath(content.id);
            if (!docker || releasePath.empty()) {
                NOVA_LOG("[LaravelOrchestrator] DockerOrchestrator or an active materialized release is unavailable.", LogType::Warning);
                continue;
            }
            const auto job = docker->SubmitComposeJob(DockerComposeJobAction::Logs, releasePath, content.composeFile);
            NOVA_LOG(("[LaravelOrchestrator] Log job for '" + content.id + "' is " + job.id + ".").c_str(),
                     (job.state == DockerComposeJobState::Accepted || job.state == DockerComposeJobState::Running) ? LogType::Log : LogType::Error);
        } else if (argument.Flag == "remote-bootstrap" || argument.Flag == "remote-content-status") {
            RemoteTarget target;
            if (!LoadRemoteTarget(content, target, error)) {
                NOVA_LOG(("[LaravelOrchestrator] " + error).c_str(), LogType::Error);
                continue;
            }
            auto* docker = dynamic_cast<IDockerOrchestrator*>(
                Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("dockerorchestrator"));
            if (!docker) {
                NOVA_LOG("[LaravelOrchestrator] DockerOrchestrator is not loaded.", LogType::Error);
                continue;
            }
            DockerRemoteTarget dockerTarget{target.host, target.port, target.user, target.knownHostsFile};
            const auto jobId = argument.Flag == "remote-bootstrap"
                ? docker->BootstrapRemoteAsync(dockerTarget, [contentId = content.id](DockerComposeResult commandResult) {
                    NOVA_LOG(("[LaravelOrchestrator] Remote Docker bootstrap for '" + contentId + "' completed with exit code " + std::to_string(commandResult.exitCode) + ".").c_str(), commandResult.succeeded ? LogType::Log : LogType::Error);
                })
                : docker->QueryRemoteComposeAsync(dockerTarget, target.releaseRoot + "/current", [contentId = content.id](DockerComposeResult commandResult) {
                    NOVA_LOG(("[LaravelOrchestrator] Remote Docker status for '" + contentId + "' completed with exit code " + std::to_string(commandResult.exitCode) + ".").c_str(), commandResult.succeeded ? LogType::Log : LogType::Error);
                });
            NOVA_LOG(("[LaravelOrchestrator] Remote command queued as " + jobId + ".").c_str(), jobId.empty() ? LogType::Error : LogType::Log);
        }
    }
}

Core::CanvasMenuActionResult LaravelOrchestratorModule::OnMenuAction(const Core::CanvasMenuActionRequest& request) {
    Core::CanvasMenuActionResult result;
    const bool isLaravelMenu = request.MenuId == "laravel_main" ||
        request.MenuId == "laravelorchestrator::laravel_main";
    if (!isLaravelMenu) {
        result.Success = false;
        return result;
    }

    NOVA_LOG(("[LaravelOrchestrator] Handling menu action: " + request.ActionId).c_str(), LogType::Log);

    auto* contentForge = dynamic_cast<Core::IContentForge*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("contentforge"));
    if (!contentForge) {
        result.Success = false;
        result.ErrorMessage = "ContentForge is unavailable.";
        return result;
    }

    Core::LocalContentDescriptor content;
    if (!ResolveRequestedLaravelContent(request, contentForge, content, result.ErrorMessage)) {
        result.Success = false;
        return result;
    }

    const auto contentId = content.id;
    const std::string statusText = "Runtime Status: ";
    if (request.ActionId == "laravel.novaid.begin") {
        std::string loginUrl;
        result.Success = BeginNovaIdLogin(content, loginUrl, result.ErrorMessage);
        if (result.Success) {
            result.ConfigUpdates["novaIdStatus"] = "Nova ID: Login Required - complete the external flow";
            result.ConfigUpdates["novaIdLoginUrl"] = "OAuth Device URL: " + loginUrl;
            PublishDeploymentToast("NOVAID_LOGIN_LINK_READY", "Nova ID login URL was generated. Complete it in the browser or Postman, then refresh login.", Core::CanvasNotificationSeverity::Info);
        }
        return result;
    }
    if (request.ActionId == "laravel.novaid.poll") {
        std::string pollStatus;
        result.Success = PollNovaIdLogin(content, pollStatus, result.ErrorMessage);
        if (result.Success) {
            result.ConfigUpdates["novaIdStatus"] = "Nova ID: " + pollStatus;
            PublishDeploymentToast("NOVAID_POLL_QUEUED", "Nova ID status poll was queued; the menu remains usable.", Core::CanvasNotificationSeverity::Info);
        }
        return result;
    }
    if (request.ActionId == "laravel.novaid.logout") {
        LogoutNovaId(contentId);
        result.ConfigUpdates["novaIdStatus"] = "Nova ID: Login Required";
        result.ConfigUpdates["novaIdLoginUrl"] = "Login URL: Waiting for KeyForge OAuth credential materialization";
        PublishDeploymentToast("NOVAID_LOCAL_SESSION_FORGOTTEN", "The local in-memory Nova ID session was removed.", Core::CanvasNotificationSeverity::Info);
        return result;
    }

    auto* docker = dynamic_cast<IDockerOrchestrator*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("dockerorchestrator"));
    if (!docker) {
        result.Success = false;
        result.ErrorMessage = "DockerOrchestrator is unavailable.";
        return result;
    }

    if (request.ActionId.rfind("laravel.remote.", 0) == 0) {
        NovaIdLoginContract contract;
        bool requiresNovaId = false;
        std::string contractError;
        if (!LoadNovaIdLoginContract(content, contract, requiresNovaId, contractError)) {
            result.Success = false;
            result.ErrorMessage = contractError;
            return result;
        }
        if (requiresNovaId && !HasNovaIdToken(contentId)) {
            result.Success = false;
            result.ErrorMessage = "Remote control requires Nova ID login. Generate the login URL, complete it, then refresh login.";
            result.ConfigUpdates["novaIdStatus"] = "Nova ID: Login Required for remote control";
            return result;
        }
    }

    if (request.ActionId == "laravel.remote.stage") {
        const auto deployment = DeployRemoteContent(contentId);
        result.Success = deployment.succeeded;
        result.ErrorMessage = deployment.succeeded ? "" : deployment.message;
        result.ConfigUpdates["remoteStatus"] = deployment.succeeded ? "Remote Status: Staging queued" : "Remote Status: Stage failed";
        if (deployment.succeeded) {
            PublishDeploymentToast("REMOTE_STAGE_QUEUED", deployment.message, Core::CanvasNotificationSeverity::Info);
        }
        return result;
    }

    if (request.ActionId == "laravel.remote.bootstrap" || request.ActionId == "laravel.remote.refresh") {
        RemoteTarget target;
        if (!LoadRemoteTarget(content, target, result.ErrorMessage)) {
            result.Success = false;
            return result;
        }
        if (!docker) {
            result.Success = false;
            result.ErrorMessage = "DockerOrchestrator is unavailable.";
            return result;
        }
        const bool isBootstrap = request.ActionId == "laravel.remote.bootstrap";
        DockerRemoteTarget dockerTarget{target.host, target.port, target.user, target.knownHostsFile};
        const auto jobId = isBootstrap
            ? docker->BootstrapRemoteAsync(dockerTarget, [contentId](DockerComposeResult commandResult) {
                NOVA_LOG(("[LaravelOrchestrator] Remote Docker bootstrap UI command for '" + contentId + "' completed with exit code " + std::to_string(commandResult.exitCode) + ".").c_str(), commandResult.succeeded ? LogType::Log : LogType::Error);
            })
            : docker->QueryRemoteComposeAsync(dockerTarget, target.releaseRoot + "/current", [contentId](DockerComposeResult commandResult) {
                NOVA_LOG(("[LaravelOrchestrator] Remote Docker status UI command for '" + contentId + "' completed with exit code " + std::to_string(commandResult.exitCode) + ".").c_str(), commandResult.succeeded ? LogType::Log : LogType::Error);
            });
        result.Success = !jobId.empty();
        result.ErrorMessage = result.Success ? "" : "TerminalAgent rejected the remote command.";
        result.ConfigUpdates["remoteStatus"] = result.Success
            ? std::string("Remote Status: ") + (isBootstrap ? "Bootstrap queued" : "Refresh queued") + " (job " + jobId + ")"
            : "Remote Status: Command rejected";
        if (result.Success) {
            PublishDeploymentToast(isBootstrap ? "REMOTE_BOOTSTRAP_QUEUED" : "REMOTE_STATUS_QUEUED",
                                  "Remote action for '" + contentId + "' was queued as " + jobId + ".",
                                  Core::CanvasNotificationSeverity::Info);
        }
        return result;
    }

    if (request.ActionId == "laravel.content.start") {
        std::string jobId;
        result.Success = QueueLocalDeployment(contentId, "auto", jobId);
        result.ErrorMessage = result.Success ? "" : "A deployment for this content is already queued.";
        result.ConfigUpdates["contentStatus"] = result.Success ? statusText + "Queued (job " + jobId + ")" : statusText + "Already queued";
        result.ConfigUpdates["contentDetails"] = DescribeContent(content) + " Deployment runs in the background.";
        result.ConfigUpdates["contentProgress"] = "5";
        if (result.Success) {
            PublishDeploymentToast("CONTENT_START_QUEUED", "Deployment for '" + contentId + "' was queued as " + jobId + ".", Core::CanvasNotificationSeverity::Info);
        }
        return result;
    }

    if (request.ActionId == "laravel.content.stop") {
        const auto releasePath = GetActiveReleasePath(contentId);
        if (releasePath.empty()) {
            result.Success = false;
            result.ErrorMessage = "No active materialized release is tracked for this process.";
            return result;
        }
        const auto job = docker->SubmitComposeJob(DockerComposeJobAction::Stop, releasePath, content.composeFile);
        result.Success = job.state == DockerComposeJobState::Accepted || job.state == DockerComposeJobState::Running;
        result.ErrorMessage = result.Success ? "" : job.output;
        result.ConfigUpdates["contentStatus"] = result.Success ? statusText + "Stopping (job " + job.id + ")" : statusText + "Stop failed";
        result.ConfigUpdates["contentDetails"] = DescribeContent(content);
        if (result.Success) {
            PublishDeploymentToast("CONTENT_STOPPING", "Stop action for '" + contentId + "' was queued as " + job.id + ".", Core::CanvasNotificationSeverity::Info);
        }
        return result;
    }

    if (request.ActionId == "laravel.content.refresh") {
        if (content.primaryService.empty()) {
            result.Success = false;
            result.ErrorMessage = "The selected content pack has no deployment.primaryService configured.";
            return result;
        }
        const auto releasePath = GetActiveReleasePath(contentId);
        if (releasePath.empty()) {
            result.Success = false;
            result.ErrorMessage = "No active materialized release is tracked for this process.";
            return result;
        }
        result.ConfigUpdates["contentStatus"] = docker->IsComposeServiceRunning(releasePath, content.primaryService, content.composeFile)
            ? statusText + "Running"
            : statusText + "Stopped";
        result.ConfigUpdates["contentDetails"] = DescribeContent(content);
        return result;
    }

    result.Success = false;
    result.ErrorMessage = "Unsupported Laravel menu action.";
    return result;
}

LaravelDeploymentResult LaravelOrchestratorModule::DeployLocalContent(const std::string& contentId, const std::string& profile) {
    LaravelDeploymentResult deployment;
    const auto fail = [&contentId](const std::string& phase) {
        Core::ProgressTracker::Publish({"deploy:" + contentId, "laravelorchestrator", phase, 100, false, true});
    };
    Core::ProgressTracker::Publish({"deploy:" + contentId, "laravelorchestrator", "Resolving ContentForge pack", 20, true});
    auto* contentForge = dynamic_cast<Core::IContentForge*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("contentforge"));
    if (!contentForge) {
        deployment.message = "ContentForge is not loaded.";
        fail(deployment.message);
        return deployment;
    }

    Core::LocalContentDescriptor content;
    if (!ResolveLaravelContent(contentId, contentForge, content, deployment.message)) {
        fail(deployment.message);
        return deployment;
    }

    if (!IsConfigurationDepth(profile)) {
        deployment.message = "Configuration depth must be auto, minimal, normal, or advanced.";
        fail(deployment.message);
        return deployment;
    }
    auto* docker = dynamic_cast<IDockerOrchestrator*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("dockerorchestrator"));
    if (!docker) {
        deployment.message = "DockerOrchestrator is not loaded.";
        fail(deployment.message);
        return deployment;
    }

    Core::LocalContentRelease release;
    Core::LocalContentDescriptor releaseContent;
    if (!MaterializeLaravelRelease(contentForge, content, release, releaseContent, deployment.message)) {
        fail("Materialization failed: " + deployment.message);
        return deployment;
    }
    Core::ProgressTracker::Publish({"deploy:" + contentId, "laravelorchestrator", "Injecting KeyForge runtime environment", 55, true});
    if (!InjectRuntimeEnvironment(content, release, deployment.message)) {
        fail("Runtime environment injection failed: " + deployment.message);
        return deployment;
    }
    Core::ProgressTracker::Publish({"deploy:" + contentId, "laravelorchestrator", "Installing or validating Docker runtime", 70, true});
    const auto bootstrap = docker->BootstrapLocalRuntime();
    if (!bootstrap.succeeded) {
        deployment.message = "Docker runtime bootstrap failed: " + bootstrap.output;
        fail(deployment.message);
        return deployment;
    }
    const auto previousReleasePath = GetActiveReleasePath(content.id);
    if (!previousReleasePath.empty() && previousReleasePath != release.releasePath &&
        std::filesystem::is_directory(previousReleasePath)) {
        Core::ProgressTracker::Publish({"deploy:" + contentId, "laravelorchestrator", "Stopping previous local release", 82, true});
        const auto stopPrevious = docker->StopCompose(previousReleasePath, content.composeFile);
        if (!stopPrevious.succeeded) {
            deployment.message = "Could not stop the previous local release: " + stopPrevious.output;
            fail(deployment.message);
            return deployment;
        }
    }
    Core::ProgressTracker::Publish({"deploy:" + contentId, "laravelorchestrator", "Starting Docker Compose services", 85, true});
    // CLI/service deployments are one-shot transactions.  Waiting here is
    // essential: an async child would be terminated when the one-shot
    // process exits, leaving no Compose project running.
    const auto start = docker->StartComposeWithProgress(release.releasePath,
        [&contentId](int percent, const std::string& activity) {
            Core::ProgressTracker::Publish({"deploy:" + contentId, "laravelorchestrator", activity, percent, true});
        }, content.composeFile);
    deployment.succeeded = start.succeeded;
    if (deployment.succeeded) {
        Core::ProgressTracker::Publish({"deploy:" + contentId, "laravelorchestrator", "Repairing Laravel writable paths", 99, true});
        if (!RepairSailWritablePaths(release, content, deployment.message)) {
            deployment.succeeded = false;
        }
        if (deployment.succeeded) {
            Core::ProgressTracker::Publish({"deploy:" + contentId, "laravelorchestrator", "Migrating and bootstrapping local Auth API", 99, true});
            if (!BootstrapAuthApiLocalDatabase(release, content, deployment.message)) {
                deployment.succeeded = false;
            }
        }
    }
    deployment.message = deployment.succeeded
        ? "Local Laravel release '" + release.releaseId + "' started successfully (configuration depth: " + profile + ")."
        : start.output;
    if (deployment.succeeded) {
        RememberActiveRelease(content.id, release.releasePath);
    }
    Core::ProgressTracker::Publish({"deploy:" + contentId, "laravelorchestrator",
        deployment.succeeded ? "Docker Compose started" : "Docker Compose failed", 100, false, !deployment.succeeded});
    return deployment;
}

LaravelDeploymentResult LaravelOrchestratorModule::DeployRemoteContent(const std::string& contentId, const std::string& profile) {
    LaravelDeploymentResult deployment;
    if (!IsConfigurationDepth(profile)) {
        deployment.message = "Configuration depth must be auto, minimal, normal, or advanced.";
        return deployment;
    }

    auto* contentForge = dynamic_cast<Core::IContentForge*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("contentforge"));
    Core::LocalContentDescriptor content;
    if (!ResolveLaravelContent(contentId, contentForge, content, deployment.message)) {
        return deployment;
    }

    NovaIdLoginContract contract;
    bool requiresNovaId = false;
    if (!LoadNovaIdLoginContract(content, contract, requiresNovaId, deployment.message)) {
        return deployment;
    }
    if (requiresNovaId && !HasNovaIdToken(contentId)) {
        deployment.message = "Remote deployment requires an authenticated Nova ID session.";
        return deployment;
    }

    RemoteTarget target;
    if (!LoadRemoteTarget(content, target, deployment.message)) {
        return deployment;
    }
    auto* terminal = ResolveTerminalAgent();
    if (!terminal) {
        deployment.message = "TerminalAgent is not loaded.";
        return deployment;
    }

    Core::LocalContentRelease release;
    Core::LocalContentDescriptor releaseContent;
    if (!MaterializeLaravelRelease(contentForge, content, release, releaseContent, deployment.message)) {
        return deployment;
    }

    // ContentForge excludes .env and key material. KeyForge must inject the
    // runtime environment in a later step, before Compose may be started.
    const auto remoteReleasePath = target.releaseRoot + "/" + content.id + "/releases/" + release.releaseId;
    CoreTerminal::RemoteDirectoryUploadRequest uploadRequest;
    uploadRequest.host = target.host;
    uploadRequest.port = target.port;
    uploadRequest.user = target.user;
    uploadRequest.knownHostsFile = target.knownHostsFile;
    uploadRequest.localDirectory = release.releasePath;
    uploadRequest.remoteDirectory = remoteReleasePath;
    const auto jobId = terminal->UploadDirectoryAsync(uploadRequest, [contentId, remoteReleasePath](CoreTerminal::TerminalCommandResult uploadResult) {
        NOVA_LOG(("[LaravelOrchestrator] Remote stage for '" + contentId + "' at " + remoteReleasePath +
                  " completed with exit code " + std::to_string(uploadResult.exitCode) + ".").c_str(),
                 uploadResult.exitCode == 0 ? LogType::Log : LogType::Error);
    });
    if (jobId.empty()) {
        deployment.message = "TerminalAgent rejected the remote upload request.";
        return deployment;
    }

    deployment.succeeded = true;
    deployment.message = "Remote release '" + release.releaseId + "' was staged as " + jobId +
        ". KeyForge environment injection and compose activation remain separate required actions.";
    return deployment;
}
