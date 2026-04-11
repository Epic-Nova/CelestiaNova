#include "RabbitMQOrchestrator.h"

#include "Core/NovaLog.h"

RabbitMQOrchestratorModule::RabbitMQOrchestratorModule() {}
RabbitMQOrchestratorModule::~RabbitMQOrchestratorModule() {}

void RabbitMQOrchestratorModule::StartupModule() {
    NOVA_LOG("[RabbitMQOrchestrator] StartupModule called", LogType::Log);
}

void RabbitMQOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[RabbitMQOrchestrator] ShutdownModule called", LogType::Log);
}
