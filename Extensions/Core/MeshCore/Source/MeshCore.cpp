#include "MeshCore.h"
#include "MeshCoreClientDelegate.h"

#include "Core/FTSTicker.h"
#include "Core/NovaLog.h"

MeshCoreModule::MeshCoreModule() {}
MeshCoreModule::~MeshCoreModule() {}

void MeshCoreModule::StartupModule() {
    NOVA_LOG("[MeshCore] StartupModule called", LogType::Log);

    // Register a FTSTicker delegate to poll client-mode job results every 2 seconds.
    // TODO: instantiate and store a MeshClientDelegateImpl, then poll it here.
    // The delegate returns false to self-unsubscribe on shutdown.
    TickerHandle_ = Core::FTSTicker::GetCoreTicker().AddTicker([](float) -> bool {
        // TODO: call ClientDelegate_->PollJobResults() once client mode is active.
        // Return true to keep ticking, false to stop.
        return true;
    }, 2.0f /*poll every 2 seconds*/);

    NOVA_LOG("[MeshCore] Registered FTSTicker delegate for client-mode polling.", LogType::Log);
}

void MeshCoreModule::ShutdownModule() {
    NOVA_LOG("[MeshCore] ShutdownModule called", LogType::Log);
    Core::FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle_);
    // TODO: DisconnectFromAuthoritativeInstance() on the active client delegate.
}

Core::NovaCapabilityDescriptor MeshCoreModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "meshcore";
    descriptor.displayName = "MeshCore";
    descriptor.description = "Federation and remote-management core for host/client Celestia Nova mode.";
    descriptor.serviceCapabilities = { "mesh.discovery", "mesh.remote.execute", "mesh.node.list" };
    descriptor.healthEndpoints = { "/api/v1/health/meshcore" };
    descriptor.contentPacks = { "MeshClientMode" };
    descriptor.telemetryStreams = { "mesh.remote.calls", "mesh.node.count" };
    descriptor.grafanaDashboards = { "grafana://celestianova/meshcore-federation" };
    return descriptor;
}

Core::NovaHealthSnapshot MeshCoreModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "MeshCore base module initialized";
    return health;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, MeshCoreModule)
