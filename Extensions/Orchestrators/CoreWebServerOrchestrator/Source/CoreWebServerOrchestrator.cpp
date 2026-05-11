#include "CoreWebServerOrchestrator.h"

#include "Core/NovaLog.h"

CoreWebServerOrchestratorModule::CoreWebServerOrchestratorModule() {}
CoreWebServerOrchestratorModule::~CoreWebServerOrchestratorModule() {}

void CoreWebServerOrchestratorModule::StartupModule() {
    NOVA_LOG("[CoreWebServerOrchestrator] StartupModule called", LogType::Log);
}

void CoreWebServerOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[CoreWebServerOrchestrator] ShutdownModule called", LogType::Log);
}

Core::NovaCapabilityDescriptor CoreWebServerOrchestratorModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "corewebserverorchestrator";
    descriptor.displayName = "CoreWebServerOrchestrator";
    descriptor.description = "Base orchestrator for web server environments.";
    descriptor.serviceCapabilities = { "orchestrator.webserver.setup", "orchestrator.webserver.config" };
    return descriptor;
}

Core::NovaHealthSnapshot CoreWebServerOrchestratorModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "CoreWebServerOrchestrator base module initialized";
    return health;
}

std::string CoreWebServerOrchestratorModule::GenerateVirtualHostConfig(const CoreWebServer::WebServerHostConfig& config) const {
    std::string result = "server {\n";
    result += "    server_name " + config.domainName + ";\n";
    if (config.enableSSL) {
        result += "    listen 443 ssl;\n";
        result += "    ssl_certificate /etc/letsencrypt/live/" + config.domainName + "/fullchain.pem;\n";
        result += "    ssl_certificate_key /etc/letsencrypt/live/" + config.domainName + "/privkey.pem;\n";
    } else {
        result += "    listen 80;\n";
    }
    
    if (!config.documentRoot.empty()) {
        result += "    root " + config.documentRoot + ";\n";
        result += "    index index.html index.php;\n";
    }
    
    if (!config.upstreamHost.empty()) {
        result += GetProxyDirectives(config.upstreamHost, config.upstreamPort);
    }
    
    result += "}\n";
    return result;
}

std::string CoreWebServerOrchestratorModule::GetProxyDirectives(const std::string& upstreamHost, int port) const {
    std::string result = "    location / {\n";
    result += "        proxy_pass http://" + upstreamHost + ":" + std::to_string(port) + ";\n";
    result += "        proxy_set_header Host $host;\n";
    result += "        proxy_set_header X-Real-IP $remote_addr;\n";
    result += "        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;\n";
    result += "    }\n";
    return result;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, CoreWebServerOrchestratorModule)
