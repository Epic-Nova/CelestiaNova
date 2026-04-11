#include "NodeAgent.h"

#include "Core/NovaLog.h"

NodeAgentModule::NodeAgentModule() {}
NodeAgentModule::~NodeAgentModule() {}

void NodeAgentModule::StartupModule() {
    NOVA_LOG("[NodeAgent] StartupModule called", LogType::Log);
}

void NodeAgentModule::ShutdownModule() {
    NOVA_LOG("[NodeAgent] ShutdownModule called", LogType::Log);
}
