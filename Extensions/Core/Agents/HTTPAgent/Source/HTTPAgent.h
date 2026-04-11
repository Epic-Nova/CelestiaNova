#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef HTTPAgent_EXPORTS
#  define HTTPAGENT_API NOVA_EXPORT
#else
#  define HTTPAGENT_API NOVA_IMPORT
#endif

class HTTPAGENT_API HTTPAgentModule : public IExtensionInterface {
public:
    HTTPAgentModule();
    ~HTTPAgentModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef HTTPAgent_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(HTTPAGENT_API, HTTPAgentModule)
#endif

