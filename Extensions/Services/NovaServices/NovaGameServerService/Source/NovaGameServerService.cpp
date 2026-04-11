#include "NovaGameServerService.h"

#include "Core/NovaLog.h"

NovaGameServerServiceModule::NovaGameServerServiceModule() {}
NovaGameServerServiceModule::~NovaGameServerServiceModule() {}

void NovaGameServerServiceModule::StartupModule() {
    NOVA_LOG("[NovaGameServerService] StartupModule called", LogType::Log);
}

void NovaGameServerServiceModule::ShutdownModule() {
    NOVA_LOG("[NovaGameServerService] ShutdownModule called", LogType::Log);
}
