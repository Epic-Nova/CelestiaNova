#include "NexusCore.h"

#include "Core/NovaLog.h"
#include "Core/ProgressTracker.h"
#include "Core/StatusApiSurface.h"
#include "Core/ExtensionRegistry.h"
#include "ExtensionSpecific/ISignalCoreSurfaces.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <json.hpp>
#include <sstream>

namespace {

std::string NowUtcIso8601() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &timestamp);
#else
    utc = *std::gmtime(&timestamp);
#endif
    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

} // namespace

NexusCoreModule::NexusCoreModule() {
    // Mesh membership endpoints are deployment configuration, never a
    // hard-coded management host in the binary.
}

NexusCoreModule::~NexusCoreModule() {}

void NexusCoreModule::StartupModule() {
    NOVA_LOG("[NexusCore] StartupModule called. Nexus status aggregation active.", LogType::Log);
    
    // Periodically report instance state (scaffolding)
    ReportToAuthoritativeInstances();
}

void NexusCoreModule::ShutdownModule() {
    NOVA_LOG("[NexusCore] ShutdownModule called.", LogType::Log);
}

void NexusCoreModule::ReportToAuthoritativeInstances() {
    NOVA_LOG("[NexusCore] Status aggregation is available for MeshCore's authenticated membership reporter.", LogType::Log);
}

int NexusCoreModule::GetStatusRoutingPolicyPriority() const {
    return 100;
}

bool NexusCoreModule::AcceptsProviderForDomain(Core::StatusDeclarationDomain domain,
                                               const std::string& providerId) const {
    if (providerId.empty()) {
        return false;
    }

    // NexusCore owns declaration routing policy so domain ownership can live
    // in extensions instead of hardcoded optional-key parsing in Core.
    switch (domain) {
        case Core::StatusDeclarationDomain::HealthEndpoints:
            return providerId == "nexuscore";
        case Core::StatusDeclarationDomain::ContentEndpoints:
            return providerId == "nexuscore";
        case Core::StatusDeclarationDomain::GrafanaDashboards:
            return providerId == "pulsecore";
        case Core::StatusDeclarationDomain::ServiceCapabilities:
            return providerId == "nexuscore";
        case Core::StatusDeclarationDomain::ContentPacks:
            return providerId == "contentforge";
        case Core::StatusDeclarationDomain::TelemetryStreams:
            return providerId == "pulsecore";
        default:
            return true;
    }
}

Core::NovaCapabilityDescriptor NexusCoreModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "nexuscore";
    descriptor.displayName = "NexusCore";
    descriptor.description = "Service-side extension registry and status aggregation core for frontend and API consumption.";
    descriptor.serviceCapabilities = {
        "extensions.list",
        "services.list",
        "status.snapshot",
        "contentpacks.list",
        "status.routing.policy"
    };
    descriptor.healthEndpoints = { "/api/v1/health/nexuscore" };
    descriptor.contentPacks = { "NexusStatusAPI" };
    descriptor.contentEndpoints = { "/api/v1/content/nexuscore", "/api/v1/content/nexuscore/index" };
    descriptor.telemetryStreams = { "nexus.requests", "nexus.snapshot.size" };
    descriptor.grafanaDashboards = { "grafana://celestianova/nexuscore-overview" };
    return descriptor;
}

Core::NovaHealthSnapshot NexusCoreModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "NexusCore base module initialized";
    return health;
}

Core::NovaInstanceConnectivitySnapshot NexusCoreModule::GetInstanceConnectivitySnapshot() const {
    auto* mesh = dynamic_cast<Core::IInstanceConnectivityProvider*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("meshcore"));
    if (mesh) return mesh->GetInstanceConnectivitySnapshot();

    Core::NovaInstanceConnectivitySnapshot snapshot;
    snapshot.ProviderId = "nexuscore";
    snapshot.Role = Core::NovaInstanceConnectivityRole::Standalone;
    snapshot.ConnectedInstanceCount = 0;
    snapshot.Summary = "No MeshCore connectivity provider is currently available.";
    return snapshot;
}

std::string NexusCoreModule::BuildDaemonStatusJson() const {
    nlohmann::json payload;
    payload["schema"] = "celestianova.daemon-status.v1";
    payload["generatedAtUtc"] = NowUtcIso8601();
    payload["extensions"] = nlohmann::json::parse(Core::StatusApiSurface::BuildExtensionsStatusJson(), nullptr, false);
    if (payload["extensions"].is_discarded()) payload["extensions"] = nlohmann::json::array();

    const auto progress = Core::ProgressTracker::Read();
    payload["progress"] = {
        {"operationId", progress.operationId}, {"owner", progress.owner},
        {"phase", progress.phase}, {"percent", progress.percent}, {"active", progress.active}
    };

    const auto connectivity = GetInstanceConnectivitySnapshot();
    payload["connectivity"] = {
        {"providerId", connectivity.ProviderId},
        {"connectedInstanceCount", connectivity.ConnectedInstanceCount},
        {"summary", connectivity.Summary}
    };

    payload["capabilities"] = {
        {"healthEndpoints", Core::StatusApiSurface::ListDeclaredHealthEndpoints()},
        {"contentEndpoints", Core::StatusApiSurface::ListDeclaredContentEndpoints()},
        {"serviceCapabilities", Core::StatusApiSurface::ListDeclaredServiceCapabilities()},
        {"telemetryStreams", Core::StatusApiSurface::ListDeclaredTelemetryStreams()}
    };

    payload["signals"] = nlohmann::json::array();
    auto* signalBus = dynamic_cast<Core::ISignalNotificationBus*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("signalcore"));
    if (signalBus) {
        std::uint64_t latest = 0;
        for (const auto& envelope : signalBus->ConsumeSignalNotifications(0, 32, latest)) {
            payload["signals"].push_back({
                {"sequence", envelope.Sequence},
                {"channel", envelope.Notification.Channel},
                {"sourceExtensionId", envelope.Notification.SourceExtensionId},
                {"title", envelope.Notification.Title},
                {"message", envelope.Notification.Message},
                {"createdAtUtc", envelope.Notification.CreatedAtUtc}
            });
        }
        payload["signalsLatestSequence"] = latest;
    }
    return payload.dump();
}

std::vector<std::string> NexusCoreModule::GetRemoteInstances() const {
    return {};
}

bool NexusCoreModule::DispatchRemoteCommand(const std::string& instanceId, const std::string& command) {
    NOVA_LOG(("[NexusCore] Remote command dispatch is owned by MeshCore; rejected request for " + instanceId + ".").c_str(), LogType::Warning);
    return false;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, NexusCoreModule)
