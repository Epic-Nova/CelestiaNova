#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"

#ifdef CoreWebServerOrchestrator_EXPORTS
#  define COREWEBSERVERORCHESTRATOR_API NOVA_EXPORT
#else
#  define COREWEBSERVERORCHESTRATOR_API NOVA_IMPORT
#endif

#include <string>
#include <vector>

namespace CoreWebServer {

struct WebServerHostConfig {
    std::string domainName;
    std::string documentRoot;
    std::string upstreamHost;
    int upstreamPort = 80;
    bool enableSSL = true;
};

class ICoreWebServerOrchestrator {
public:
    virtual ~ICoreWebServerOrchestrator() = default;

    virtual std::string GenerateVirtualHostConfig(const WebServerHostConfig& config) const = 0;
    virtual std::string GetProxyDirectives(const std::string& upstreamHost, int port) const = 0;
};

} // namespace CoreWebServer

class COREWEBSERVERORCHESTRATOR_API CoreWebServerOrchestratorModule : 
    public IExtensionInterface, 
    public Core::INovaCapabilityProvider,
    public CoreWebServer::ICoreWebServerOrchestrator {
public:
    CoreWebServerOrchestratorModule();
    ~CoreWebServerOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;

    // ICoreWebServerOrchestrator Implementation
    std::string GenerateVirtualHostConfig(const CoreWebServer::WebServerHostConfig& config) const override;
    std::string GetProxyDirectives(const std::string& upstreamHost, int port) const override;
};
