#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"

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

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;
};
