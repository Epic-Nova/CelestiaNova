#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"

#ifdef NovaID_EXPORTS
#  define NOVAID_API NOVA_EXPORT
#else
#  define NOVAID_API NOVA_IMPORT
#endif

class NOVAID_API NovaIDModule : public IExtensionInterface, public Core::INovaCapabilityProvider {
public:
    NovaIDModule();
    ~NovaIDModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;
};
