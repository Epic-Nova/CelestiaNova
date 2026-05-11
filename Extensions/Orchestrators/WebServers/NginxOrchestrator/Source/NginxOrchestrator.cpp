#include "NginxOrchestrator.h"

#include "Core/NovaLog.h"

NginxOrchestratorModule::NginxOrchestratorModule() {}
NginxOrchestratorModule::~NginxOrchestratorModule() {}

#include "Core/ExtensionRegistry.h"
#include "../../CoreWebServerOrchestrator/Source/CoreWebServerOrchestrator.h"

void NginxOrchestratorModule::StartupModule() {
    NOVA_LOG("[NginxOrchestrator] StartupModule called. Requesting virtual host config generation...", LogType::Log);

    auto* webServerCore = dynamic_cast<CoreWebServer::ICoreWebServerOrchestrator*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("corewebserverorchestrator"));
    
    if (webServerCore) {
        CoreWebServer::WebServerHostConfig config;
        config.domainName = "api.celestianova.local";
        config.upstreamHost = "172.18.0.5";
        config.upstreamPort = 8000;
        config.enableSSL = true;
        
        std::string generatedConfig = webServerCore->GenerateVirtualHostConfig(config);
        NOVA_LOG(("[NginxOrchestrator] Generated Nginx Config:\n" + generatedConfig).c_str(), LogType::Log);
    } else {
        NOVA_LOG("[NginxOrchestrator] Failed to load CoreWebServerOrchestrator dependency.", LogType::Warning);
    }
}

void NginxOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[NginxOrchestrator] ShutdownModule called", LogType::Log);
}
