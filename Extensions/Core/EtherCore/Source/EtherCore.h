#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"

class EtherCoreModule : public IExtensionInterface,
                        public Core::INovaCapabilityProvider {
public:
    EtherCoreModule();
    ~EtherCoreModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    // INovaCapabilityProvider
    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;
};
