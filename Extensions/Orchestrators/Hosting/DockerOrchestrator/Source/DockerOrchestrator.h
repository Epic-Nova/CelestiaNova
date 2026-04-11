#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef DockerOrchestrator_EXPORTS
#  define DOCKERORCHESTRATOR_API NOVA_EXPORT
#else
#  define DOCKERORCHESTRATOR_API NOVA_IMPORT
#endif

class DOCKERORCHESTRATOR_API DockerOrchestratorModule : public IExtensionInterface {
public:
    DockerOrchestratorModule();
    ~DockerOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef DockerOrchestrator_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(DOCKERORCHESTRATOR_API, DockerOrchestratorModule)
#endif

