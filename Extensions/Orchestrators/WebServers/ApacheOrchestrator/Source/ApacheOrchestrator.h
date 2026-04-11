#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef ApacheOrchestrator_EXPORTS
#  define APACHEORCHESTRATOR_API NOVA_EXPORT
#else
#  define APACHEORCHESTRATOR_API NOVA_IMPORT
#endif

class APACHEORCHESTRATOR_API ApacheOrchestratorModule : public IExtensionInterface {
public:
    ApacheOrchestratorModule();
    ~ApacheOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef ApacheOrchestrator_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(APACHEORCHESTRATOR_API, ApacheOrchestratorModule)
#endif

