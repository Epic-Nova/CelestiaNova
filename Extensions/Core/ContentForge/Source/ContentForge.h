#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"

#ifdef ContentForge_EXPORTS
#  define CONTENTFORGE_API NOVA_EXPORT
#else
#  define CONTENTFORGE_API NOVA_IMPORT
#endif

class CONTENTFORGE_API ContentForgeModule : public IExtensionInterface, public Core::INovaCapabilityProvider {
public:
    ContentForgeModule();
    ~ContentForgeModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;
};
