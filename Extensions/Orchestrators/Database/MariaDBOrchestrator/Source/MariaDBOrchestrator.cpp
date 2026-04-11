#include "MariaDBOrchestrator.h"

#include "Core/NovaLog.h"

MariaDBOrchestratorModule::MariaDBOrchestratorModule() {}
MariaDBOrchestratorModule::~MariaDBOrchestratorModule() {}

void MariaDBOrchestratorModule::StartupModule() {
    NOVA_LOG("[MariaDBOrchestrator] StartupModule called", LogType::Log);
}

void MariaDBOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[MariaDBOrchestrator] ShutdownModule called", LogType::Log);
}
