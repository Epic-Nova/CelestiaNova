#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef NovaAPIService_EXPORTS
#  define NOVAAPISERVICE_API NOVA_EXPORT
#else
#  define NOVAAPISERVICE_API NOVA_IMPORT
#endif

class NOVAAPISERVICE_API NovaAPIServiceModule : public IExtensionInterface {
public:
    NovaAPIServiceModule();
    ~NovaAPIServiceModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef NovaAPIService_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(NOVAAPISERVICE_API, NovaAPIServiceModule)
#endif

