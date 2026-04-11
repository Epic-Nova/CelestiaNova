#include "PrivilegeEscalationAgent.h"

#include "Core/NovaLog.h"

PrivilegeEscalationAgentModule::PrivilegeEscalationAgentModule() {}
PrivilegeEscalationAgentModule::~PrivilegeEscalationAgentModule() {}

void PrivilegeEscalationAgentModule::StartupModule() {
    NOVA_LOG("[PrivilegeEscalationAgent] StartupModule called", LogType::Log);
}

void PrivilegeEscalationAgentModule::ShutdownModule() {
    NOVA_LOG("[PrivilegeEscalationAgent] ShutdownModule called", LogType::Log);
}
