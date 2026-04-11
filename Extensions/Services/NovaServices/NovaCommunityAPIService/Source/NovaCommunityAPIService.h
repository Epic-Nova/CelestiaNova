#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef NovaCommunityAPIService_EXPORTS
#  define NOVACOMMUNITYAPISERVICE_API NOVA_EXPORT
#else
#  define NOVACOMMUNITYAPISERVICE_API NOVA_IMPORT
#endif

class NOVACOMMUNITYAPISERVICE_API NovaCommunityAPIServiceModule : public IExtensionInterface {
public:
    NovaCommunityAPIServiceModule();
    ~NovaCommunityAPIServiceModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef NovaCommunityAPIService_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(NOVACOMMUNITYAPISERVICE_API, NovaCommunityAPIServiceModule)
#endif

