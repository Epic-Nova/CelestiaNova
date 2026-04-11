#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef NodeAgent_EXPORTS
#  define NODEAGENT_API NOVA_EXPORT
#else
#  define NODEAGENT_API NOVA_IMPORT
#endif

class NODEAGENT_API NodeAgentModule : public IExtensionInterface {
public:
    NodeAgentModule();
    ~NodeAgentModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef NodeAgent_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(NODEAGENT_API, NodeAgentModule)
#endif

