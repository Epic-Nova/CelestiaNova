#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef PrivilegeEscalationAgent_EXPORTS
#  define PRIVILEGEESCALATIONAGENT_API NOVA_EXPORT
#else
#  define PRIVILEGEESCALATIONAGENT_API NOVA_IMPORT
#endif

class PRIVILEGEESCALATIONAGENT_API PrivilegeEscalationAgentModule : public IExtensionInterface {
public:
    PrivilegeEscalationAgentModule();
    ~PrivilegeEscalationAgentModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef PrivilegeEscalationAgent_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(PRIVILEGEESCALATIONAGENT_API, PrivilegeEscalationAgentModule)
#endif

