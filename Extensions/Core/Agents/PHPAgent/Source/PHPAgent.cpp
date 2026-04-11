#include "PHPAgent.h"

#include "Core/NovaLog.h"

PHPAgentModule::PHPAgentModule() {}
PHPAgentModule::~PHPAgentModule() {}

void PHPAgentModule::StartupModule() {
    NOVA_LOG("[PHPAgent] StartupModule called", LogType::Log);
}

void PHPAgentModule::ShutdownModule() {
    NOVA_LOG("[PHPAgent] ShutdownModule called", LogType::Log);
}
