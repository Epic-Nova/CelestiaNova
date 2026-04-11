#include "GitAgent.h"

#include "Core/NovaLog.h"

GitAgentModule::GitAgentModule() {}
GitAgentModule::~GitAgentModule() {}

void GitAgentModule::StartupModule() {
    NOVA_LOG("[GitAgent] StartupModule called", LogType::Log);
}

void GitAgentModule::ShutdownModule() {
    NOVA_LOG("[GitAgent] ShutdownModule called", LogType::Log);
}
