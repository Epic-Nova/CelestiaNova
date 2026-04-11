#include "DockerOrchestrator.h"

#include "Core/NovaLog.h"

DockerOrchestratorModule::DockerOrchestratorModule() {}
DockerOrchestratorModule::~DockerOrchestratorModule() {}

void DockerOrchestratorModule::StartupModule() {
    NOVA_LOG("[DockerOrchestrator] StartupModule called", LogType::Log);
}

void DockerOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[DockerOrchestrator] ShutdownModule called", LogType::Log);
}
