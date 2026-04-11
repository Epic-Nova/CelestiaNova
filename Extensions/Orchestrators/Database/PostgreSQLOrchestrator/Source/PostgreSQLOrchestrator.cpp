#include "PostgreSQLOrchestrator.h"

#include "Core/NovaLog.h"

PostgreSQLOrchestratorModule::PostgreSQLOrchestratorModule() {}
PostgreSQLOrchestratorModule::~PostgreSQLOrchestratorModule() {}

void PostgreSQLOrchestratorModule::StartupModule() {
    NOVA_LOG("[PostgreSQLOrchestrator] StartupModule called", LogType::Log);
}

void PostgreSQLOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[PostgreSQLOrchestrator] ShutdownModule called", LogType::Log);
}
