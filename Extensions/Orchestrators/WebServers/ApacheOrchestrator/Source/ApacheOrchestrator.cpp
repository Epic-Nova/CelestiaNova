#include "ApacheOrchestrator.h"

#include "Core/NovaLog.h"

ApacheOrchestratorModule::ApacheOrchestratorModule() {}
ApacheOrchestratorModule::~ApacheOrchestratorModule() {}

void ApacheOrchestratorModule::StartupModule() {
    NOVA_LOG("[ApacheOrchestrator] StartupModule called", LogType::Log);
}

void ApacheOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[ApacheOrchestrator] ShutdownModule called", LogType::Log);
}
