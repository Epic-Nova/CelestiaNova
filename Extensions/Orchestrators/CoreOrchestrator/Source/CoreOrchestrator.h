#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"
#include "ExtensionSpecific/IOrchestrationSurfaces.h"

#ifdef CoreOrchestrator_EXPORTS
#  define COREORCHESTRATOR_API NOVA_EXPORT
#else
#  define COREORCHESTRATOR_API NOVA_IMPORT
#endif

class COREORCHESTRATOR_API CoreOrchestratorModule :
    public IExtensionInterface,
    public Core::INovaCapabilityProvider,
    public Core::IOrchestratorSetupProfileProvider,
    public Core::IOrchestratorInteractionLifecycleProvider {
public:
    CoreOrchestratorModule();
    ~CoreOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;

    std::vector<Core::SetupProfileDepth> GetSupportedProfileDepths() const override;
    Core::OrchestratorSetupSurface BuildSetupSurface(const Core::SetupSurfaceRequest& request,
                                                     Core::SetupProfileDepth depth) const override;
    Core::InteractionLifecycleContract GetInteractionLifecycleContract() const override;
};

#ifdef CoreOrchestrator_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(COREORCHESTRATOR_API, CoreOrchestratorModule)
#endif

