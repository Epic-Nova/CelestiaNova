#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef SurrealDBOrchestrator_EXPORTS
#  define SURREALDBORCHESTRATOR_API NOVA_EXPORT
#else
#  define SURREALDBORCHESTRATOR_API NOVA_IMPORT
#endif

class SURREALDBORCHESTRATOR_API SurrealDBOrchestratorModule : public IExtensionInterface {
public:
    SurrealDBOrchestratorModule();
    ~SurrealDBOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef SurrealDBOrchestrator_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(SURREALDBORCHESTRATOR_API, SurrealDBOrchestratorModule)
#endif

