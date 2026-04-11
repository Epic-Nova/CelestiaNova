#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef PostgreSQLOrchestrator_EXPORTS
#  define POSTGRESQLORCHESTRATOR_API NOVA_EXPORT
#else
#  define POSTGRESQLORCHESTRATOR_API NOVA_IMPORT
#endif

class POSTGRESQLORCHESTRATOR_API PostgreSQLOrchestratorModule : public IExtensionInterface {
public:
    PostgreSQLOrchestratorModule();
    ~PostgreSQLOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef PostgreSQLOrchestrator_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(POSTGRESQLORCHESTRATOR_API, PostgreSQLOrchestratorModule)
#endif

