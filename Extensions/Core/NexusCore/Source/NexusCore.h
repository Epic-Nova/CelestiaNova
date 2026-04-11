#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"
#include "ExtensionSpecific/IStatusRoutingPolicyProvider.h"

#ifdef NexusCore_EXPORTS
#  define NEXUSCORE_API NOVA_EXPORT
#else
#  define NEXUSCORE_API NOVA_IMPORT
#endif

class NEXUSCORE_API NexusCoreModule : public IExtensionInterface,
                                      public Core::INovaCapabilityProvider,
                                      public Core::IStatusRoutingPolicyProvider {
public:
    NexusCoreModule();
    ~NexusCoreModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;

    int GetStatusRoutingPolicyPriority() const override;
    bool AcceptsProviderForDomain(Core::StatusDeclarationDomain domain,
                                  const std::string& providerId) const override;
};
