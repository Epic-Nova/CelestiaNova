#include "CoreDNSOrchestrator.h"

#include "Core/NovaLog.h"

CoreDNSOrchestratorModule::CoreDNSOrchestratorModule() {}
CoreDNSOrchestratorModule::~CoreDNSOrchestratorModule() {}

void CoreDNSOrchestratorModule::StartupModule() {
    NOVA_LOG("[CoreDNSOrchestrator] StartupModule called", LogType::Log);
}

void CoreDNSOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[CoreDNSOrchestrator] ShutdownModule called", LogType::Log);
}

Core::NovaCapabilityDescriptor CoreDNSOrchestratorModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "corednsorchestrator";
    descriptor.displayName = "CoreDNSOrchestrator";
    descriptor.description = "CoreDNS setup and configuration orchestrator.";
    descriptor.serviceCapabilities = { "coredns.install", "coredns.configure", "coredns.health" };
    descriptor.healthEndpoints = { "/api/v1/health/corednsorchestrator" };
    descriptor.contentPacks = { "CoreDNSSetup" };
    descriptor.telemetryStreams = { "corednsorchestrator.actions" };
    descriptor.grafanaDashboards = { "grafana://celestianova/coredns-orchestrator" };
    return descriptor;
}

Core::NovaHealthSnapshot CoreDNSOrchestratorModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "CoreDNSOrchestrator base module initialized";
    return health;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, CoreDNSOrchestratorModule)
