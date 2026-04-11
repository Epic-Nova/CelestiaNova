#include "TraefikOrchestrator.h"

#include "Core/NovaLog.h"

TraefikOrchestratorModule::TraefikOrchestratorModule() {}
TraefikOrchestratorModule::~TraefikOrchestratorModule() {}

void TraefikOrchestratorModule::StartupModule() {
    NOVA_LOG("[TraefikOrchestrator] StartupModule called", LogType::Log);
}

void TraefikOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[TraefikOrchestrator] ShutdownModule called", LogType::Log);
}

Core::NovaCapabilityDescriptor TraefikOrchestratorModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "traefikorchestrator";
    descriptor.displayName = "TraefikOrchestrator";
    descriptor.description = "Traefik reverse proxy orchestrator for dynamic routing and edge integration.";
    descriptor.serviceCapabilities = { "traefik.install", "traefik.configure", "traefik.health" };
    descriptor.healthEndpoints = { "/api/v1/health/traefikorchestrator" };
    descriptor.contentPacks = { "TraefikSetup" };
    descriptor.telemetryStreams = { "traefikorchestrator.actions" };
    descriptor.grafanaDashboards = { "grafana://celestianova/traefik-orchestrator" };
    return descriptor;
}

Core::NovaHealthSnapshot TraefikOrchestratorModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "TraefikOrchestrator base module initialized";
    return health;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, TraefikOrchestratorModule)
