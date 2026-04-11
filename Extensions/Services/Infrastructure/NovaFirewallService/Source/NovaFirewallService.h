#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"

#ifdef NovaFirewallService_EXPORTS
#  define NOVAFIREWALLSERVICE_API NOVA_EXPORT
#else
#  define NOVAFIREWALLSERVICE_API NOVA_IMPORT
#endif

class NOVAFIREWALLSERVICE_API NovaFirewallServiceModule : public IExtensionInterface, public Core::INovaCapabilityProvider {
public:
    NovaFirewallServiceModule();
    ~NovaFirewallServiceModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;
};
