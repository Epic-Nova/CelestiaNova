#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"
#include "ExtensionSpecific/IPersistenceSurfaces.h"
#include "ExtensionSpecific/IOrchestrationSurfaces.h"

#ifdef AstraScaleCore_EXPORTS
#  define ASTRASCALECORE_API NOVA_EXPORT
#else
#  define ASTRASCALECORE_API NOVA_IMPORT
#endif

class ASTRASCALECORE_API AstraScaleCoreModule :
    public IExtensionInterface,
    public Core::INovaCapabilityProvider,
    public Core::IOrchestratorSetupProfileProvider,
    public Core::IOrchestratorInteractionLifecycleProvider,
    public Core::INovaPersistenceSurface,
    public Core::INovaPersistenceSurfaceProvider {
public:
    AstraScaleCoreModule();
    ~AstraScaleCoreModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;

    std::vector<Core::SetupProfileDepth> GetSupportedProfileDepths() const override;
    Core::OrchestratorSetupSurface BuildSetupSurface(const Core::SetupSurfaceRequest& request,
                                                     Core::SetupProfileDepth depth) const override;
    Core::InteractionLifecycleContract GetInteractionLifecycleContract() const override;

    std::string GetPersistenceSurfaceId() const override;
    bool ReadRecord(const Core::PersistenceReadRequest& request, Core::PersistenceRecord& outRecord) const override;
    bool WriteRecord(const Core::PersistenceWriteRequest& request) const override;
    std::vector<Core::PersistenceRecord> ListRecords(const Core::PersistenceListRequest& request) const override;
    std::vector<Core::PersistenceBinding> GetPersistenceBindings() const override;
};

#ifdef AstraScaleCore_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(ASTRASCALECORE_API, AstraScaleCoreModule)
#endif
