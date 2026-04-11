#include "TerminalAgent.h"

#include "Core/NovaLog.h"

TerminalAgentModule::TerminalAgentModule() {}
TerminalAgentModule::~TerminalAgentModule() {}

void TerminalAgentModule::StartupModule() {
    NOVA_LOG("[TerminalAgent] StartupModule called", LogType::Log);
}

void TerminalAgentModule::ShutdownModule() {
    NOVA_LOG("[TerminalAgent] ShutdownModule called", LogType::Log);
}
