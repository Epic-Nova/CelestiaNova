#include "NovaCDNService.h"

#include "Core/NovaLog.h"

NovaCDNServiceModule::NovaCDNServiceModule() {}
NovaCDNServiceModule::~NovaCDNServiceModule() {}

void NovaCDNServiceModule::StartupModule() {
    NOVA_LOG("[NovaCDNService] StartupModule called", LogType::Log);
}

void NovaCDNServiceModule::ShutdownModule() {
    NOVA_LOG("[NovaCDNService] ShutdownModule called", LogType::Log);
}
