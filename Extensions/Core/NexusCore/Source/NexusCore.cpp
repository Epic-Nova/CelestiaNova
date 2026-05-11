#include "NexusCore.h"

#include "Core/NovaLog.h"

NexusCoreModule::NexusCoreModule() {
    // [Scaffolding] Default authoritative management endpoints for the cluster
    AuthoritativeEndpoints_ = { "https://mgmt.epicnova.net/api/v1/instances/report" };
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
    NOVA_LOG("[NexusCore] Gathering instance status snapshot for authoritative reporting...", LogType::Log);
    
    for (const auto& endpoint : AuthoritativeEndpoints_) {
        NOVA_LOG(("[NexusCore] Reporting status to management hub: " + endpoint).c_str(), LogType::Log);
        // Implement secure HTTP POST with aggregated status JSON
    }
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
    Core::NovaInstanceConnectivitySnapshot snapshot;
    snapshot.ProviderId = "nexuscore";
    snapshot.Role = Core::NovaInstanceConnectivityRole::Host;
    snapshot.ConnectedInstanceCount = 5; // [Scaffolding]
    snapshot.Summary = "NexusCore: Discovered 5 peer instances via MeshCore.";
    return snapshot;
}

std::vector<std::string> NexusCoreModule::GetRemoteInstances() const {
    // [Scaffolding] Mock remote instance list
    return { "remote-host-01", "remote-host-02", "remote-host-03" };
}

bool NexusCoreModule::DispatchRemoteCommand(const std::string& instanceId, const std::string& command) {
    NOVA_LOG(("[NexusCore] Dispatching remote command '" + command + "' to instance " + instanceId).c_str(), LogType::Log);
    return true;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, NexusCoreModule)
