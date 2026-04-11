#include "SurrealDBOrchestrator.h"

#include "Core/NovaLog.h"

SurrealDBOrchestratorModule::SurrealDBOrchestratorModule() {}
SurrealDBOrchestratorModule::~SurrealDBOrchestratorModule() {}

void SurrealDBOrchestratorModule::StartupModule() {
    NOVA_LOG("[SurrealDBOrchestrator] StartupModule called", LogType::Log);
}

void SurrealDBOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[SurrealDBOrchestrator] ShutdownModule called", LogType::Log);
}
