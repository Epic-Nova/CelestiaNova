#include "ContentForge.h"

#include "Core/NovaLog.h"
#include "Core/ExtensionRegistry.h"
#include "Core/RequirementResolver.h"
#include "ExtensionSpecific/IPackageManagerAgent.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <json.hpp>

namespace {

using ContentForgeResolveRequest = Core::RequirementResolver::CoreRequirementResolveRequest;
using ContentForgeResolveResult = Core::RequirementResolver::CoreRequirementResolveResult;

std::string ReadContextValue(const ContentForgeResolveRequest& request, const std::string& key) {
    for (const auto& [contextKey, contextValue] : request.ContextValues) {
        if (contextKey == key) {
            return contextValue;
        }
    }
    return {};
}

bool IsSafePathSegment(const std::string& value) {
    if (value.empty() || value == "." || value == "..") {
        return false;
    }

    for (const unsigned char character : value) {
        if (!(std::isalnum(character) || character == '-' || character == '_' || character == '.')) {
            return false;
        }
    }
    return true;
}

bool IsSafeHttpsUrl(const std::string& value) {
    return value.rfind("https://", 0) == 0 &&
           value.find_first_of(" \t\r\n\"'`|&;<>") == std::string::npos;
}

bool IsSafeRemotePath(const std::string& value) {
    return !value.empty() && value.front() == '/' && value.find("..") == std::string::npos &&
           value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789/_-.") == std::string::npos;
}

std::string ToLower(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

bool ShouldExcludeFromRelease(const std::filesystem::path& relativePath, bool isDirectory) {
    const auto name = ToLower(relativePath.filename().string());
    // Windows device names cannot be safely recreated inside a release. They
    // are never valid application assets and commonly appear as accidental
    // Git/editor artifacts (for example a literal file named NUL).
    if (name == "con" || name == "prn" || name == "aux" || name == "nul" ||
        (name.size() == 4 && name.rfind("com", 0) == 0 && name[3] >= '1' && name[3] <= '9') ||
        (name.size() == 4 && name.rfind("lpt", 0) == 0 && name[3] >= '1' && name[3] <= '9')) {
        return true;
    }
    if (isDirectory) {
        return name == ".git" || name == "node_modules" || name == ".idea" || name == ".vscode";
    }

    // Source .env files and private key material must enter through a later,
    // explicitly audited injection stage; they are never copied into releases.
    if (name == ".env" || (name.rfind(".env.", 0) == 0 && name != ".env.example")) {
        return true;
    }
    const auto extension = ToLower(relativePath.extension().string());
    return extension == ".key" || extension == ".pem" || extension == ".pfx" || extension == ".p12";
}

std::string CreateReleaseId() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return "local-" + std::to_string(milliseconds);
}

bool IsWithin(const std::filesystem::path& candidate, const std::filesystem::path& root) {
    const auto relative = candidate.lexically_relative(root);
    const auto genericRelative = relative.generic_string();
    return !genericRelative.empty() && genericRelative.rfind("..", 0) != 0;
}

std::filesystem::path ResolveRuntimeRoot(const std::filesystem::path& applicationRoot) {
    const char* configuredRoot = std::getenv("CELESTIA_RUNTIME_ROOT");
    if (configuredRoot != nullptr && configuredRoot[0] == '/') {
        return std::filesystem::path(configuredRoot);
    }
    return applicationRoot / "Content" / ".runtime";
}

bool RegisterLocalManifest(const std::filesystem::path& manifestPath,
                           const std::filesystem::path& applicationRoot,
                           ContentForgeModule& forge) {
    std::ifstream manifestFile(manifestPath);
    if (!manifestFile) {
        return false;
    }

    try {
        nlohmann::json manifest;
        manifestFile >> manifest;
        const auto source = manifest.value("source", nlohmann::json::object());
        if (source.value("type", "") != "local-path") {
            return false;
        }

        Core::LocalContentDescriptor content;
        content.id = manifest.value("id", "");
        content.version = manifest.value("version", "");
        content.framework = manifest.value("framework", "");
        const auto deployment = manifest.value("deployment", nlohmann::json::object());
        content.orchestrator = deployment.value("orchestrator", "");
        content.composeFile = deployment.value("composeFile", "");
        content.primaryService = deployment.value("primaryService", "");
        content.healthEndpoint = deployment.value("healthEndpoint", "");
        content.manifestPath = manifestPath.string();

        if (content.id.empty() || content.version.empty() || content.framework.empty() ||
            content.orchestrator.empty() || content.composeFile.empty() || content.primaryService.empty()) {
            NOVA_LOG((std::string("[ContentForge] Content manifest is missing required fields: ") + manifestPath.string()).c_str(), LogType::Warning);
            return false;
        }

        const auto sourceBase = source.value("base", "application-root");
        const auto basePath = sourceBase == "manifest-directory" ? manifestPath.parent_path() : applicationRoot;
        content.path = (basePath / source.value("path", "")).string();
        const auto localDevelopment = manifest.value("localDevelopment", nlohmann::json::object());
        const auto environmentFile = localDevelopment.value("environmentFile", "");
        if (!environmentFile.empty()) {
            const auto environmentBase = localDevelopment.value("base", "application-root") == "manifest-directory"
                ? manifestPath.parent_path()
                : applicationRoot;
            content.localEnvironmentFile = (environmentBase / environmentFile).string();
        }
        return forge.RegisterLocalContent(content);
    } catch (const std::exception& error) {
        NOVA_LOG((std::string("[ContentForge] Failed to read content manifest '") + manifestPath.string() + "': " + error.what()).c_str(), LogType::Warning);
        return false;
    }
}

} // namespace

extern "C" CONTENTFORGE_API bool ContentForge_ResolveRequirement(const void* requestPtr, void* resultPtr) {
    return Core::RequirementResolver::DispatchResolveRequest(requestPtr, resultPtr,
        [](const ContentForgeResolveRequest& request) {
            ContentForgeResolveResult result;
            if (request.RequirementKey != "contentforge.local.content") {
                result.ErrorCode = "UnsupportedRequirement";
                result.ErrorMessage = "ContentForge does not support requirement key '" + request.RequirementKey + "'.";
                return result;
            }

            auto* forge = dynamic_cast<Core::IContentForge*>(
                Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("contentforge"));
            if (!forge) {
                result.ErrorCode = "ContentForgeUnavailable";
                result.ErrorMessage = "ContentForge is not loaded.";
                return result;
            }

            const std::string requestedFramework = ToLower(ReadContextValue(request, "framework"));
            const std::string requestedOrchestrator = ToLower(ReadContextValue(request, "orchestrator"));
            for (const auto& content : forge->ListLocalContent()) {
                if (!requestedFramework.empty() && ToLower(content.framework) != requestedFramework) {
                    continue;
                }
                if (!requestedOrchestrator.empty() && ToLower(content.orchestrator) != requestedOrchestrator) {
                    continue;
                }

                Core::RequirementResolver::CoreRequirementResolvedOption option;
                option.Value = content.id;
                option.Label = content.id + " (" + content.framework + ", " + content.version + ")";
                option.Description = "Local ContentForge pack for " + content.orchestrator + ".";
                result.Options.push_back(std::move(option));
            }

            if (result.Options.empty()) {
                result.ErrorCode = "NoOptions";
                result.ErrorMessage = "No local content packs match this menu.";
                return result;
            }

            result.Success = true;
            return result;
        });
}

ContentForgeModule::ContentForgeModule() {
    std::lock_guard<std::mutex> lock(ProviderMutex_);
    ContentProviders_ = { "LocalFileSystem", "GitRemote", "S3Static" };
}

ContentForgeModule::~ContentForgeModule() {}

void ContentForgeModule::StartupModule() {
    NOVA_LOG("[ContentForge] StartupModule called", LogType::Log);
    const auto applicationRoot = std::filesystem::current_path();
    ApplicationRoot_ = applicationRoot.string();
    std::vector<std::filesystem::path> manifestPaths;
    const auto collectManifests = [&manifestPaths](const std::filesystem::path& root) {
        std::error_code error;
        if (!std::filesystem::is_directory(root, error)) {
            return;
        }
        for (const auto& entry : std::filesystem::directory_iterator(root, error)) {
            if (!error && entry.is_regular_file() && entry.path().extension() == ".json") {
                manifestPaths.push_back(entry.path());
            }
        }
    };

    collectManifests(applicationRoot / "Content" / "ContentPacks");
    const auto extensionsRoot = applicationRoot / "Extensions";
    std::error_code error;
    if (std::filesystem::is_directory(extensionsRoot, error)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(extensionsRoot, error)) {
            if (!error && entry.is_directory() && entry.path().filename() == "Content") {
                collectManifests(entry.path() / "ContentPacks");
            }
        }
    }

    std::size_t registered = 0;
    for (const auto& manifestPath : manifestPaths) {
        registered += RegisterLocalManifest(manifestPath, applicationRoot, *this) ? 1 : 0;
    }
    NOVA_LOG(("[ContentForge] Discovered " + std::to_string(registered) + " local content pack(s).").c_str(), LogType::Log);
}

void ContentForgeModule::ShutdownModule() {
    NOVA_LOG("[ContentForge] ShutdownModule called", LogType::Log);
}

Core::NovaCapabilityDescriptor ContentForgeModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "contentforge";
    descriptor.displayName = "ContentForge";
    descriptor.description = "Content pack and extension asset lifecycle core.";
    descriptor.serviceCapabilities = { "content.fetch", "content.install", "content.list" };
    descriptor.healthEndpoints = { "/api/v1/health/contentforge" };
    descriptor.contentPacks = { "ContentPackRuntime" };
    descriptor.telemetryStreams = { "contentforge.fetch.count", "contentforge.pack.count" };
    descriptor.grafanaDashboards = { "grafana://celestianova/contentforge-delivery" };
    return descriptor;
}

Core::NovaHealthSnapshot ContentForgeModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "ContentForge base module initialized";
    return health;
}

bool ContentForgeModule::Clone(const std::string& url, const std::string& destination, const std::string& branch) {
    return FetchViaGitAgent(url, destination);
}

bool ContentForgeModule::Pull(const std::string& repoPath) {
    NOVA_LOG(("[ContentForge] Pulling latest for " + repoPath).c_str(), LogType::Log);
    return true;
}

bool ContentForgeModule::Push(const std::string& repoPath, const std::string& message) {
    NOVA_LOG(("[ContentForge] Pushing changes for " + repoPath + " with message: " + message).c_str(), LogType::Log);
    return true;
}

std::string ContentForgeModule::GetCurrentBranch(const std::string& repoPath) {
    return "main";
}

bool ContentForgeModule::IsRepo(const std::string& path) {
    return true;
}

bool ContentForgeModule::MountFileSystemPath(const std::string& sourcePath, const std::string& mountTarget) const {
    NOVA_LOG(("[ContentForge] Mounting file system path '" + sourcePath + "' to target '" + mountTarget + "'.").c_str(), LogType::Log);
    return true;
}

bool ContentForgeModule::FetchViaGitAgent(const std::string& repoUrl, const std::string& destination) const {
    NOVA_LOG(("[ContentForge] Fetching content via GitAgent from '" + repoUrl + "' into '" + destination + "'.").c_str(), LogType::Log);
    
    auto& registry = Core::ExtensionRegistry::Instance();
    auto* git = dynamic_cast<Core::ISourceControlAgent*>(registry.GetLoadedExtensionInstance("gitagent"));
    if (git) {
        return git->Clone(repoUrl, destination);
    }
    
    return false;
}

std::vector<std::string> ContentForgeModule::GetContentProviders() const {
    std::lock_guard<std::mutex> lock(ProviderMutex_);
    return ContentProviders_;
}

bool ContentForgeModule::RegisterLocalContent(const Core::LocalContentDescriptor& descriptor) {
    if (descriptor.id.empty() || descriptor.path.empty()) {
        NOVA_LOG("[ContentForge] Local content registration requires an id and path.", LogType::Warning);
        return false;
    }

    std::error_code error;
    const auto path = std::filesystem::weakly_canonical(descriptor.path, error);
    if (error || !std::filesystem::is_directory(path)) {
        NOVA_LOG(("[ContentForge] Local content path is not a directory: " + descriptor.path).c_str(), LogType::Warning);
        return false;
    }

    auto normalized = descriptor;
    normalized.path = path.string();
    std::lock_guard<std::mutex> lock(ProviderMutex_);
    LocalContent_[normalized.id] = std::move(normalized);
    NOVA_LOG(("[ContentForge] Registered local content: " + descriptor.id).c_str(), LogType::Log);
    return true;
}

bool ContentForgeModule::ResolveLocalContent(const std::string& contentId, Core::LocalContentDescriptor& outDescriptor) const {
    std::lock_guard<std::mutex> lock(ProviderMutex_);
    const auto iterator = LocalContent_.find(contentId);
    if (iterator == LocalContent_.end()) {
        return false;
    }

    outDescriptor = iterator->second;
    return true;
}

std::vector<Core::LocalContentDescriptor> ContentForgeModule::ListLocalContent() const {
    std::lock_guard<std::mutex> lock(ProviderMutex_);
    std::vector<Core::LocalContentDescriptor> content;
    content.reserve(LocalContent_.size());
    for (const auto& [id, descriptor] : LocalContent_) {
        content.push_back(descriptor);
    }
    return content;
}

bool ContentForgeModule::MaterializeLocalContent(const std::string& contentId,
                                                  const std::string& requestedReleaseId,
                                                  Core::LocalContentRelease& outRelease) {
    outRelease = {};
    if (!IsSafePathSegment(contentId)) {
        NOVA_LOG("[ContentForge] Refusing to materialize an unsafe content id.", LogType::Warning);
        return false;
    }

    const auto releaseId = requestedReleaseId.empty() ? CreateReleaseId() : requestedReleaseId;
    if (!IsSafePathSegment(releaseId)) {
        NOVA_LOG("[ContentForge] Refusing to materialize an unsafe release id.", LogType::Warning);
        return false;
    }

    Core::LocalContentDescriptor content;
    if (!ResolveLocalContent(contentId, content)) {
        NOVA_LOG(("[ContentForge] Cannot materialize unknown local content: " + contentId).c_str(), LogType::Warning);
        return false;
    }

    std::error_code error;
    const auto sourcePath = std::filesystem::weakly_canonical(content.path, error);
    if (error || !std::filesystem::is_directory(sourcePath, error)) {
        NOVA_LOG(("[ContentForge] Cannot materialize unavailable source path: " + content.path).c_str(), LogType::Warning);
        return false;
    }

    const auto applicationRoot = ApplicationRoot_.empty() ? std::filesystem::current_path() : std::filesystem::path(ApplicationRoot_);
    const auto runtimeRoot = ResolveRuntimeRoot(applicationRoot);
    std::filesystem::create_directories(runtimeRoot, error);
    if (error) {
        NOVA_LOG(("[ContentForge] Cannot create runtime root: " + runtimeRoot.string()).c_str(), LogType::Warning);
        return false;
    }
    const auto canonicalRuntimeRoot = std::filesystem::weakly_canonical(runtimeRoot, error);
    if (error) {
        NOVA_LOG("[ContentForge] Cannot canonicalize runtime root.", LogType::Warning);
        return false;
    }

    const auto releasePath = canonicalRuntimeRoot / contentId / releaseId;
    if (!IsWithin(releasePath.lexically_normal(), canonicalRuntimeRoot) || std::filesystem::exists(releasePath, error)) {
        NOVA_LOG(("[ContentForge] Release already exists or escapes runtime root: " + releaseId).c_str(), LogType::Warning);
        return false;
    }
    std::filesystem::create_directories(releasePath, error);
    if (error) {
        NOVA_LOG(("[ContentForge] Cannot create release directory: " + releasePath.string()).c_str(), LogType::Warning);
        return false;
    }

    bool copied = true;
    std::filesystem::recursive_directory_iterator iterator(sourcePath, std::filesystem::directory_options::skip_permission_denied, error);
    const std::filesystem::recursive_directory_iterator end;
    for (; !error && iterator != end; iterator.increment(error)) {
        const auto entry = *iterator;
        const auto relativePath = entry.path().lexically_relative(sourcePath);
        const auto status = entry.symlink_status(error);
        if (error) {
            copied = false;
            break;
        }

        if (std::filesystem::is_symlink(status)) {
            NOVA_LOG(("[ContentForge] Skipping symlink while materializing: " + relativePath.string()).c_str(), LogType::Warning);
            if (entry.is_directory(error)) {
                iterator.disable_recursion_pending();
            }
            continue;
        }
        if (entry.is_directory(error)) {
            if (ShouldExcludeFromRelease(relativePath, true)) {
                iterator.disable_recursion_pending();
                continue;
            }
            std::filesystem::create_directories(releasePath / relativePath, error);
            if (error) {
                copied = false;
                break;
            }
            continue;
        }
        if (!entry.is_regular_file(error) || ShouldExcludeFromRelease(relativePath, false)) {
            continue;
        }

        const auto target = releasePath / relativePath;
        std::filesystem::create_directories(target.parent_path(), error);
        if (!error) {
            std::filesystem::copy_file(entry.path(), target, std::filesystem::copy_options::none, error);
        }
        if (error) {
            copied = false;
            break;
        }
    }

    if (error || !copied) {
        NOVA_LOG(("[ContentForge] Failed to materialize release " + contentId + "/" + releaseId).c_str(), LogType::Warning);
        // This directory was created by this call and is inside the validated runtime root.
        std::filesystem::remove_all(releasePath, error);
        return false;
    }

    const auto metadataPath = releasePath / ".contentforge-release.json";
    std::ofstream metadataFile(metadataPath, std::ios::trunc);
    if (!metadataFile) {
        NOVA_LOG("[ContentForge] Failed to write release metadata.", LogType::Warning);
        std::filesystem::remove_all(releasePath, error);
        return false;
    }
    metadataFile << nlohmann::json{
        { "contentId", content.id },
        { "releaseId", releaseId },
        { "version", content.version },
        { "sourcePath", sourcePath.string() },
        { "manifestPath", content.manifestPath },
        { "materialization", "copy-only" },
        { "secretsResolved", false }
    }.dump(2) << '\n';
    metadataFile.close();

    outRelease.contentId = content.id;
    outRelease.releaseId = releaseId;
    outRelease.releasePath = releasePath.string();
    outRelease.sourcePath = sourcePath.string();
    outRelease.manifestPath = content.manifestPath;
    outRelease.version = content.version;
    NOVA_LOG(("[ContentForge] Materialized local release " + contentId + "/" + releaseId).c_str(), LogType::Log);
    return true;
}

bool ContentForgeModule::ResolveAllowlistedRemoteControlRequest(const std::string& targetId,
                                                                 const std::string& commandId,
                                                                 std::string& outUrl,
                                                                 std::string& outMethod,
                                                                 std::string& outRequiredCapability,
                                                                 std::string& outError) const {
    outUrl.clear();
    outMethod.clear();
    outRequiredCapability.clear();
    if (!IsSafePathSegment(targetId) || !IsSafePathSegment(commandId)) {
        outError = "Remote target and command IDs must be safe declared identifiers.";
        return false;
    }

    const auto manifestPath = std::filesystem::path(ApplicationRoot_) / "Extensions" / "Core" /
                              "MeshCore" / "Content" / "MeshTargets.json";
    std::ifstream file(manifestPath);
    if (!file) {
        outError = "Mesh target content is unavailable.";
        return false;
    }
    try {
        nlohmann::json manifest;
        file >> manifest;
        if (manifest.value("schema", "") != "celestianova.remote-control.targets.v1") {
            outError = "Mesh target content has an unsupported schema.";
            return false;
        }
        const auto targets = manifest.value("targets", nlohmann::json::array());
        for (const auto& target : targets) {
            if (target.value("id", "") != targetId || target.value("transport", "") != "https") continue;
            const auto endpoint = target.value("endpoint", "");
            const auto trustReference = target.value("serverTrustRef", "");
            if (!IsSafeHttpsUrl(endpoint) || trustReference.rfind("keyforge://", 0) != 0) {
                outError = "Remote-control target is missing verified HTTPS/KeyForge trust declarations.";
                return false;
            }
            for (const auto& command : target.value("allowedCommands", nlohmann::json::array())) {
                if (command.value("id", "") != commandId) continue;
                const auto method = command.value("method", "");
                const auto path = command.value("path", "");
                const auto capability = command.value("requiredCapability", "");
                if ((method != "GET" && method != "POST") || !IsSafeRemotePath(path) || capability.empty()) {
                    outError = "Remote-control command declaration is unsafe or incomplete.";
                    return false;
                }
                outUrl = endpoint + path;
                outMethod = method;
                outRequiredCapability = capability;
                return true;
            }
            outError = "The requested command is not allowlisted for this remote target.";
            return false;
        }
        outError = "The requested remote target is not declared in ContentForge content.";
        return false;
    } catch (const std::exception&) {
        outError = "Remote-control target content is invalid JSON.";
        return false;
    }
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, ContentForgeModule)
