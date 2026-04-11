#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"

#ifdef TraefikOrchestrator_EXPORTS
#  define TRAEFIKORCHESTRATOR_API NOVA_EXPORT
#else
#  define TRAEFIKORCHESTRATOR_API NOVA_IMPORT
#endif

class TRAEFIKORCHESTRATOR_API TraefikOrchestratorModule : public IExtensionInterface, public Core::INovaCapabilityProvider {
public:
    TraefikOrchestratorModule();
    ~TraefikOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;
};
