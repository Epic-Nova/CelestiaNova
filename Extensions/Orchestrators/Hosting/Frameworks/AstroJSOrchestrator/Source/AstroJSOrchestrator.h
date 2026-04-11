#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef AstroJSOrchestrator_EXPORTS
#  define ASTROJSORCHESTRATOR_API NOVA_EXPORT
#else
#  define ASTROJSORCHESTRATOR_API NOVA_IMPORT
#endif

class ASTROJSORCHESTRATOR_API AstroJSOrchestratorModule : public IExtensionInterface {
public:
    AstroJSOrchestratorModule();
    ~AstroJSOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef AstroJSOrchestrator_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(ASTROJSORCHESTRATOR_API, AstroJSOrchestratorModule)
#endif

