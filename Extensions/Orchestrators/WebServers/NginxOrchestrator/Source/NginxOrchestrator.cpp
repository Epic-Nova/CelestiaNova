#include "NginxOrchestrator.h"

#include "Core/NovaLog.h"

NginxOrchestratorModule::NginxOrchestratorModule() {}
NginxOrchestratorModule::~NginxOrchestratorModule() {}

void NginxOrchestratorModule::StartupModule() {
    NOVA_LOG("[NginxOrchestrator] StartupModule called", LogType::Log);
}

void NginxOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[NginxOrchestrator] ShutdownModule called", LogType::Log);
}
