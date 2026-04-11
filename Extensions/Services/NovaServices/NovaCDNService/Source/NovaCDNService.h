#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef NovaCDNService_EXPORTS
#  define NOVACDNSERVICE_API NOVA_EXPORT
#else
#  define NOVACDNSERVICE_API NOVA_IMPORT
#endif

class NOVACDNSERVICE_API NovaCDNServiceModule : public IExtensionInterface {
public:
    NovaCDNServiceModule();
    ~NovaCDNServiceModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef NovaCDNService_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(NOVACDNSERVICE_API, NovaCDNServiceModule)
#endif

