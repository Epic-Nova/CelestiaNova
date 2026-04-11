#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef MariaDBOrchestrator_EXPORTS
#  define MARIADBORCHESTRATOR_API NOVA_EXPORT
#else
#  define MARIADBORCHESTRATOR_API NOVA_IMPORT
#endif

class MARIADBORCHESTRATOR_API MariaDBOrchestratorModule : public IExtensionInterface {
public:
    MariaDBOrchestratorModule();
    ~MariaDBOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef MariaDBOrchestrator_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(MARIADBORCHESTRATOR_API, MariaDBOrchestratorModule)
#endif

