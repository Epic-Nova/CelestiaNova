#include "NovaAPIService.h"

#include "Core/NovaLog.h"

NovaAPIServiceModule::NovaAPIServiceModule() {}
NovaAPIServiceModule::~NovaAPIServiceModule() {}

void NovaAPIServiceModule::StartupModule() {
    NOVA_LOG("[NovaAPIService] StartupModule called", LogType::Log);
}

void NovaAPIServiceModule::ShutdownModule() {
    NOVA_LOG("[NovaAPIService] ShutdownModule called", LogType::Log);
}
