#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef NovaGameServerService_EXPORTS
#  define NOVAGAMESERVERSERVICE_API NOVA_EXPORT
#else
#  define NOVAGAMESERVERSERVICE_API NOVA_IMPORT
#endif

class NOVAGAMESERVERSERVICE_API NovaGameServerServiceModule : public IExtensionInterface {
public:
    NovaGameServerServiceModule();
    ~NovaGameServerServiceModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef NovaGameServerService_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(NOVAGAMESERVERSERVICE_API, NovaGameServerServiceModule)
#endif

