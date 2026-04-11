#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef Python3Agent_EXPORTS
#  define PYTHON3AGENT_API NOVA_EXPORT
#else
#  define PYTHON3AGENT_API NOVA_IMPORT
#endif

class PYTHON3AGENT_API Python3AgentModule : public IExtensionInterface {
public:
    Python3AgentModule();
    ~Python3AgentModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef Python3Agent_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(PYTHON3AGENT_API, Python3AgentModule)
#endif

