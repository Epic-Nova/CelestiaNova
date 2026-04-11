#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef NginxOrchestrator_EXPORTS
#  define NGINXORCHESTRATOR_API NOVA_EXPORT
#else
#  define NGINXORCHESTRATOR_API NOVA_IMPORT
#endif

class NGINXORCHESTRATOR_API NginxOrchestratorModule : public IExtensionInterface {
public:
    NginxOrchestratorModule();
    ~NginxOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef NginxOrchestrator_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(NGINXORCHESTRATOR_API, NginxOrchestratorModule)
#endif

