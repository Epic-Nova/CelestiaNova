#include "LaravelOrchestrator.h"

#include "Core/NovaLog.h"

LaravelOrchestratorModule::LaravelOrchestratorModule() {}
LaravelOrchestratorModule::~LaravelOrchestratorModule() {}

void LaravelOrchestratorModule::StartupModule() {
    NOVA_LOG("[LaravelOrchestrator] StartupModule called", LogType::Log);
}

void LaravelOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[LaravelOrchestrator] ShutdownModule called", LogType::Log);
}
