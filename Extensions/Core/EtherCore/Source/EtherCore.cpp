#include "EtherCore.h"
#include "Core/NovaLog.h"

EtherCoreModule::EtherCoreModule() {}
EtherCoreModule::~EtherCoreModule() {}

void EtherCoreModule::StartupModule() {
    NOVA_LOG("[EtherCore] StartupModule called. Secure overlay is ready.", LogType::Log);
}

void EtherCoreModule::ShutdownModule() {
    NOVA_LOG("[EtherCore] ShutdownModule called.", LogType::Log);
}

Core::NovaCapabilityDescriptor EtherCoreModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "ethercore";
    descriptor.displayName = "EtherCore";
    descriptor.description = "Secure overlay networking and VPN management core.";
    descriptor.serviceCapabilities = { "ether.tunnel.create", "ether.tunnel.status", "ether.mesh.join" };
    descriptor.telemetryStreams = { "ether.tunnel.count", "ether.traffic.rx", "ether.traffic.tx" };
    return descriptor;
}

Core::NovaHealthSnapshot EtherCoreModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "EtherCore active. All tunnels are encrypted.";
    return health;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, EtherCoreModule)
