#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "Core/FTSTicker.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"

#ifdef MeshCore_EXPORTS
#  define MESHCORE_API NOVA_EXPORT
#else
#  define MESHCORE_API NOVA_IMPORT
#endif

class MESHCORE_API MeshCoreModule : public IExtensionInterface, public Core::INovaCapabilityProvider {
public:
    MeshCoreModule();
    ~MeshCoreModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;

private:
    Core::FTSTicker::FDelegateHandle TickerHandle_;
};
