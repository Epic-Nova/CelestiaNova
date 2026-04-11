#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"

#ifdef RedisOrchestrator_EXPORTS
#  define REDISORCHESTRATOR_API NOVA_EXPORT
#else
#  define REDISORCHESTRATOR_API NOVA_IMPORT
#endif

class REDISORCHESTRATOR_API RedisOrchestratorModule : public IExtensionInterface, public Core::INovaCapabilityProvider {
public:
    RedisOrchestratorModule();
    ~RedisOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;
};
