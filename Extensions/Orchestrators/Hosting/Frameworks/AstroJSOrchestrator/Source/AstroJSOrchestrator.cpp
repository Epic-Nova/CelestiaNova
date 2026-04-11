#include "AstroJSOrchestrator.h"

#include "Core/NovaLog.h"

AstroJSOrchestratorModule::AstroJSOrchestratorModule() {}
AstroJSOrchestratorModule::~AstroJSOrchestratorModule() {}

void AstroJSOrchestratorModule::StartupModule() {
    NOVA_LOG("[AstroJSOrchestrator] StartupModule called", LogType::Log);
}

void AstroJSOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[AstroJSOrchestrator] ShutdownModule called", LogType::Log);
}
