#include "NexusCore.h"

#include "Core/NovaLog.h"

NexusCoreModule::NexusCoreModule() {}
NexusCoreModule::~NexusCoreModule() {}

void NexusCoreModule::StartupModule() {
    NOVA_LOG("[NexusCore] StartupModule called", LogType::Log);
}

void NexusCoreModule::ShutdownModule() {
    NOVA_LOG("[NexusCore] ShutdownModule called", LogType::Log);
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

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, NexusCoreModule)
