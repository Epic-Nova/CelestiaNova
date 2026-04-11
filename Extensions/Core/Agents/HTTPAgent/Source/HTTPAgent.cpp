#include "HTTPAgent.h"

#include "Core/NovaLog.h"

HTTPAgentModule::HTTPAgentModule() {}
HTTPAgentModule::~HTTPAgentModule() {}

void HTTPAgentModule::StartupModule() {
    NOVA_LOG("[HTTPAgent] StartupModule called", LogType::Log);
}

void HTTPAgentModule::ShutdownModule() {
    NOVA_LOG("[HTTPAgent] ShutdownModule called", LogType::Log);
}
