#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef TerminalAgent_EXPORTS
#  define TERMINALAGENT_API NOVA_EXPORT
#else
#  define TERMINALAGENT_API NOVA_IMPORT
#endif

class TERMINALAGENT_API TerminalAgentModule : public IExtensionInterface {
public:
    TerminalAgentModule();
    ~TerminalAgentModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef TerminalAgent_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(TERMINALAGENT_API, TerminalAgentModule)
#endif

