#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"

#ifdef NovaLoadBalancerService_EXPORTS
#  define NOVALOADBALANCERSERVICE_API NOVA_EXPORT
#else
#  define NOVALOADBALANCERSERVICE_API NOVA_IMPORT
#endif

class NOVALOADBALANCERSERVICE_API NovaLoadBalancerServiceModule : public IExtensionInterface, public Core::INovaCapabilityProvider {
public:
    NovaLoadBalancerServiceModule();
    ~NovaLoadBalancerServiceModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;
};
