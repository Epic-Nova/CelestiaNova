#include "WireGuardOrchestrator.h"
#include "Core/NovaLog.h"

WireGuardOrchestratorModule::WireGuardOrchestratorModule() {}
WireGuardOrchestratorModule::~WireGuardOrchestratorModule() {}

void WireGuardOrchestratorModule::StartupModule() {
    NOVA_LOG("[WireGuardOrchestrator] StartupModule called. Ready to manage tunnels.", LogType::Log);
}

void WireGuardOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[WireGuardOrchestrator] ShutdownModule called.", LogType::Log);
}

Core::NovaCapabilityDescriptor WireGuardOrchestratorModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "wireguardorchestrator";
    descriptor.displayName = "WireGuardOrchestrator";
    descriptor.description = "Orchestrator for WireGuard tunnel setup and configuration.";
    descriptor.serviceCapabilities = { "wireguard.interface.create", "wireguard.peer.add", "wireguard.key.generate", "wireguard.health" };
    return descriptor;
}

Core::NovaHealthSnapshot WireGuardOrchestratorModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "WireGuardOrchestrator ready. No active interfaces.";
    return health;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, WireGuardOrchestratorModule)
