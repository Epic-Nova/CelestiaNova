#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef ProxmoxVEOrchestrator_EXPORTS
#  define PROXMOXVEORCHESTRATOR_API NOVA_EXPORT
#else
#  define PROXMOXVEORCHESTRATOR_API NOVA_IMPORT
#endif

class PROXMOXVEORCHESTRATOR_API ProxmoxVEOrchestratorModule : public IExtensionInterface {
public:
    ProxmoxVEOrchestratorModule();
    ~ProxmoxVEOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef ProxmoxVEOrchestrator_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(PROXMOXVEORCHESTRATOR_API, ProxmoxVEOrchestratorModule)
#endif

