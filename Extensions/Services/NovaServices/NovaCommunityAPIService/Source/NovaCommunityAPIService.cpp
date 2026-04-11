#include "NovaCommunityAPIService.h"

#include "Core/NovaLog.h"

NovaCommunityAPIServiceModule::NovaCommunityAPIServiceModule() {}
NovaCommunityAPIServiceModule::~NovaCommunityAPIServiceModule() {}

void NovaCommunityAPIServiceModule::StartupModule() {
    NOVA_LOG("[NovaCommunityAPIService] StartupModule called", LogType::Log);
}

void NovaCommunityAPIServiceModule::ShutdownModule() {
    NOVA_LOG("[NovaCommunityAPIService] ShutdownModule called", LogType::Log);
}
