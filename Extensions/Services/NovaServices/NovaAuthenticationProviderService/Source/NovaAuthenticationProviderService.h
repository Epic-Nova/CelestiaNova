#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef NovaAuthenticationProviderService_EXPORTS
#  define NOVAAUTHENTICATIONPROVIDERSERVICE_API NOVA_EXPORT
#else
#  define NOVAAUTHENTICATIONPROVIDERSERVICE_API NOVA_IMPORT
#endif

class NOVAAUTHENTICATIONPROVIDERSERVICE_API NovaAuthenticationProviderServiceModule : public IExtensionInterface {
public:
    NovaAuthenticationProviderServiceModule();
    ~NovaAuthenticationProviderServiceModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef NovaAuthenticationProviderService_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(NOVAAUTHENTICATIONPROVIDERSERVICE_API, NovaAuthenticationProviderServiceModule)
#endif

