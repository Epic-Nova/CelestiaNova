#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"

#ifdef CoreDNSOrchestrator_EXPORTS
#  define COREDNSORCHESTRATOR_API NOVA_EXPORT
#else
#  define COREDNSORCHESTRATOR_API NOVA_IMPORT
#endif

class COREDNSORCHESTRATOR_API CoreDNSOrchestratorModule : public IExtensionInterface, public Core::INovaCapabilityProvider {
public:
    CoreDNSOrchestratorModule();
    ~CoreDNSOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;
};
