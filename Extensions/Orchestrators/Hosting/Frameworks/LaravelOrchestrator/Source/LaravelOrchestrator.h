#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef LaravelOrchestrator_EXPORTS
#  define LARAVELORCHESTRATOR_API NOVA_EXPORT
#else
#  define LARAVELORCHESTRATOR_API NOVA_IMPORT
#endif

class LARAVELORCHESTRATOR_API LaravelOrchestratorModule : public IExtensionInterface {
public:
    LaravelOrchestratorModule();
    ~LaravelOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef LaravelOrchestrator_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(LARAVELORCHESTRATOR_API, LaravelOrchestratorModule)
#endif

