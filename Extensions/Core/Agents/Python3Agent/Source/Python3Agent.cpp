#include "Python3Agent.h"

#include "Core/NovaLog.h"

Python3AgentModule::Python3AgentModule() {}
Python3AgentModule::~Python3AgentModule() {}

void Python3AgentModule::StartupModule() {
    NOVA_LOG("[Python3Agent] StartupModule called", LogType::Log);
}

void Python3AgentModule::ShutdownModule() {
    NOVA_LOG("[Python3Agent] ShutdownModule called", LogType::Log);
}
