#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"
#include "Utils/RateLimitManager.h"
#include <memory>

#ifdef NovaGatewayService_EXPORTS
#  define NOVAGATEWAYSERVICE_API NOVA_EXPORT
#else
#  define NOVAGATEWAYSERVICE_API NOVA_IMPORT
#endif

class NOVAGATEWAYSERVICE_API NovaGatewayServiceModule : public IExtensionInterface, public Core::INovaCapabilityProvider {
public:
    NovaGatewayServiceModule();
    ~NovaGatewayServiceModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    // Checks if a request should be allowed based on an identity key (IP or User ID)
    bool IsRequestAllowed(const std::string& identityKey);

    // Synchronizes rate limit state with other gateway instances via a DatabaseOrchestrator
    void SyncRateLimits();

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;

private:
    std::unique_ptr<Utils::RateLimitManager> RateLimitManager_;
    std::string DistributedStorageId_ = "redis-orchestrator"; // Target for rate limit sync
};
