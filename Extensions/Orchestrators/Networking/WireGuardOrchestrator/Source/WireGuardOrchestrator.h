#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/IOrchestrationSurfaces.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"

class WireGuardOrchestratorModule : public IExtensionInterface,
                                    public Core::INovaCapabilityProvider {
public:
    WireGuardOrchestratorModule();
    ~WireGuardOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    // INovaCapabilityProvider
    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;
};
