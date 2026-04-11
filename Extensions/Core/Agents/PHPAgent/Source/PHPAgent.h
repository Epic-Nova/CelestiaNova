#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef PHPAgent_EXPORTS
#  define PHPAGENT_API NOVA_EXPORT
#else
#  define PHPAGENT_API NOVA_IMPORT
#endif

class PHPAGENT_API PHPAgentModule : public IExtensionInterface {
public:
    PHPAgentModule();
    ~PHPAgentModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef PHPAgent_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(PHPAGENT_API, PHPAgentModule)
#endif

