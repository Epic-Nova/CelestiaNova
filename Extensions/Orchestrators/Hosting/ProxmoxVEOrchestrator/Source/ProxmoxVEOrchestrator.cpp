#include "ProxmoxVEOrchestrator.h"

#include "Core/NovaLog.h"

ProxmoxVEOrchestratorModule::ProxmoxVEOrchestratorModule() {}
ProxmoxVEOrchestratorModule::~ProxmoxVEOrchestratorModule() {}

void ProxmoxVEOrchestratorModule::StartupModule() {
    NOVA_LOG("[ProxmoxVEOrchestrator] StartupModule called", LogType::Log);
}

void ProxmoxVEOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[ProxmoxVEOrchestrator] ShutdownModule called", LogType::Log);
}
