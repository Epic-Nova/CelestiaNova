#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"

#ifdef PulseCore_EXPORTS
#  define PULSECORE_API NOVA_EXPORT
#else
#  define PULSECORE_API NOVA_IMPORT
#endif

class PULSECORE_API PulseCoreModule : public IExtensionInterface, public Core::INovaCapabilityProvider {
public:
    PulseCoreModule();
    ~PulseCoreModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;
};
