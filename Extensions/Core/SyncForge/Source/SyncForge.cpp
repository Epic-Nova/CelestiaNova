#include "SyncForge.h"

#include "Core/ExtensionRegistry.h"
#include "Core/NovaLog.h"
#include "Core/ProgressTracker.h"
#include "ExtensionSpecific/ISignalCoreSurfaces.h"
#include "KeyForgeDeploymentContracts.h"
#include "KeyForge.h"
#include "TerminalAgent.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <json.hpp>

namespace {

std::string EnvironmentOr(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return value && *value ? value : fallback;
}

bool IsHttpsUrl(const std::string& value) {
    return value.rfind("https://", 0) == 0 && value.find_first_of("\r\n") == std::string::npos;
}

bool IsSha256(const std::string& value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isxdigit(c) != 0; });
}

std::string ShellQuote(const std::string& value) {
    std::string quoted{"'"};
    for (const char character : value) {
        if (character == '\'') quoted += "'\\''";
        else quoted += character;
    }
    return quoted + "'";
}

void PublishUpdateSignal(const std::string& title, const std::string& message, Core::SignalNotificationSeverity severity) {
    auto* bus = dynamic_cast<Core::ISignalNotificationBus*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("signalcore"));
    if (!bus) return;
    Core::SignalNotification signal;
    signal.Channel = "syncforge.update";
    signal.SourceExtensionId = "syncforge";
    signal.Title = title;
    signal.Message = message;
    signal.Severity = severity;
    bus->PublishSignalNotification(signal);
}

} // namespace

SyncForgeModule::SyncForgeModule() {}
SyncForgeModule::~SyncForgeModule() {}

void SyncForgeModule::StartupModule() {
    NOVA_LOG("[SyncForge] StartupModule called. Secure update checks are available through KeyForge.", LogType::Log);
}

void SyncForgeModule::ShutdownModule() {
    NOVA_LOG("[SyncForge] ShutdownModule called", LogType::Log);
}

bool SyncForgeModule::PerformSecureUpdateCheck(const std::string& targetVersion) {
    auto* broker = dynamic_cast<KeyForge::IDeploymentSecretBroker*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("keyforge"));
    if (!broker) {
        std::lock_guard<std::mutex> lock(UpdateMutex_);
        LastUpdateState_ = "blocked";
        LastUpdateSummary_ = "KeyForge is unavailable; update checks fail closed.";
        return false;
    }

    const std::string authBase = EnvironmentOr("CELESTIA_AUTH_API_BASE_URL", "https://auth.api.epicnova.net");
    KeyForge::OAuthAuthenticatedRequest request;
    request.application = {"syncforge", "celestianova-syncforge", "auth-api", {"celestia.update.read"}, {"celestianova"}, 600};
    request.tokenEndpoint = authBase + "/api/v1/oauth/token";
    request.resourceUrl = authBase + "/api/v1/celestia-instances/update-manifest";
    request.headers.emplace("Accept", "application/json");
    if (!IsHttpsUrl(request.tokenEndpoint) || !IsHttpsUrl(request.resourceUrl)) {
        std::lock_guard<std::mutex> lock(UpdateMutex_);
        LastUpdateState_ = "blocked";
        LastUpdateSummary_ = "Only HTTPS Auth API update endpoints are accepted.";
        return false;
    }

    Core::ProgressTracker::Publish({"update-check", "syncforge", "Requesting signed update manifest", 20, true});
    const bool queued = broker->DispatchOAuthAuthenticatedRequest(request, [this, targetVersion](KeyForge::OAuthAuthenticatedResponse response) {
        std::string state = "failed";
        std::string summary = "Update manifest request failed.";
        Core::SignalNotificationSeverity severity = Core::SignalNotificationSeverity::Warning;
        try {
            const auto manifest = nlohmann::json::parse(response.body);
            const std::string version = manifest.value("version", "");
            const std::string packageUrl = manifest.value("package_url", "");
            const std::string sha256 = manifest.value("sha256", "");
            const std::string signatureUrl = manifest.value("signature_url", "");
            if (response.accepted && !version.empty() && IsHttpsUrl(packageUrl) && IsHttpsUrl(signatureUrl) && IsSha256(sha256)) {
                state = "manifest_verified";
                summary = "Verified update manifest for version " + version + "; queueing root-owned installer.";
                severity = Core::SignalNotificationSeverity::Info;
                auto* terminal = dynamic_cast<CoreTerminal::ITerminalAgent*>(
                    Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
                if (terminal) {
                    CoreTerminal::TerminalCommandRequest apply;
                    apply.command = "sudo -n /usr/local/lib/celestianova/queue-syncforge-update --package-url " + ShellQuote(packageUrl) +
                        " --signature-url " + ShellQuote(signatureUrl) + " --sha256 " + ShellQuote(sha256);
                    const auto jobId = terminal->ExecuteCommandAsync(apply, [](CoreTerminal::TerminalCommandResult result) {
                        const bool succeeded = result.exitCode == 0;
                        const std::string message = succeeded ? "Signed SyncForge update installed; daemon restart requested." : "SyncForge update applier failed; previous service installation remains active.";
                        Core::ProgressTracker::Publish({"update-apply", "syncforge", message, 100, false});
                        PublishUpdateSignal(succeeded ? "SYNCFORGE_UPDATE_APPLIED" : "SYNCFORGE_UPDATE_FAILED", message,
                            succeeded ? Core::SignalNotificationSeverity::Success : Core::SignalNotificationSeverity::Error);
                    });
                    if (!jobId.empty()) {
                        state = "apply_queued";
                        summary = "Verified update manifest for version " + version + "; root-owned applier job " + jobId + " is running.";
                    } else {
                        state = "blocked";
                        summary = "Verified manifest but could not queue the root-owned update applier.";
                        severity = Core::SignalNotificationSeverity::Error;
                    }
                } else {
                    state = "blocked";
                    summary = "Verified manifest but TerminalAgent is unavailable for the root-owned update applier.";
                    severity = Core::SignalNotificationSeverity::Error;
                }
            } else {
                summary = "Rejected invalid or unsigned update manifest.";
            }
        } catch (...) {
            summary = "Rejected malformed update manifest.";
        }
        {
            std::lock_guard<std::mutex> lock(UpdateMutex_);
            LastUpdateState_ = state;
            LastUpdateSummary_ = summary;
        }
        Core::ProgressTracker::Publish({"update-check", "syncforge", summary, 100, false});
        PublishUpdateSignal(state == "apply_queued" ? "SYNCFORGE_UPDATE_QUEUED" : "SYNCFORGE_UPDATE_REJECTED", summary, severity);
    });
    if (!queued) {
        std::lock_guard<std::mutex> lock(UpdateMutex_);
        LastUpdateState_ = "blocked";
        LastUpdateSummary_ = "KeyForge rejected the protected update request.";
        Core::ProgressTracker::Publish({"update-check", "syncforge", LastUpdateSummary_, 100, false});
        return false;
    }
    NOVA_LOG(("[SyncForge] Secure update check queued for " + targetVersion + ".").c_str(), LogType::Log);
    return true;
}

std::vector<Core::FExtensionCliArgDescriptor> SyncForgeModule::GetCliArgDescriptors() const {
    return {{"check-updates", "Request and verify the authenticated Celestia Nova update manifest.", false}};
}

void SyncForgeModule::ApplyCliArgs(const std::vector<Core::FExtensionCliArg>& args) {
    for (const auto& arg : args) if (arg.Flag == "check-updates") PerformSecureUpdateCheck("latest");
}

Core::NovaCapabilityDescriptor SyncForgeModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "syncforge";
    descriptor.displayName = "SyncForge";
    descriptor.description = "Authenticated update manifest verification and staged rollout coordination.";
    descriptor.serviceCapabilities = {"updates.check", "updates.manifest.verify", "updates.apply.signed"};
    descriptor.healthEndpoints = {"/api/v1/health/syncforge"};
    descriptor.telemetryStreams = {"syncforge.update.check", "syncforge.update.state"};
    return descriptor;
}

Core::NovaHealthSnapshot SyncForgeModule::GetHealthSnapshot() const {
    std::lock_guard<std::mutex> lock(UpdateMutex_);
    return {LastUpdateState_ == "failed" || LastUpdateState_ == "blocked" ? "degraded" : "healthy", LastUpdateSummary_};
}
