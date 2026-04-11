#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef GitAgent_EXPORTS
#  define GITAGENT_API NOVA_EXPORT
#else
#  define GITAGENT_API NOVA_IMPORT
#endif

class GITAGENT_API GitAgentModule : public IExtensionInterface {
public:
    GitAgentModule();
    ~GitAgentModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef GitAgent_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(GITAGENT_API, GitAgentModule)
#endif

