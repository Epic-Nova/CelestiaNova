#include "NovaAuthenticationProviderService.h"

#include "Core/NovaLog.h"

NovaAuthenticationProviderServiceModule::NovaAuthenticationProviderServiceModule() {}
NovaAuthenticationProviderServiceModule::~NovaAuthenticationProviderServiceModule() {}

void NovaAuthenticationProviderServiceModule::StartupModule() {
    NOVA_LOG("[NovaAuthenticationProviderService] StartupModule called", LogType::Log);
}

void NovaAuthenticationProviderServiceModule::ShutdownModule() {
    NOVA_LOG("[NovaAuthenticationProviderService] ShutdownModule called", LogType::Log);
}
