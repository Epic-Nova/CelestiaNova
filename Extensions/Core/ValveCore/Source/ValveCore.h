#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"
#include "ExtensionSpecific/IValvePolicyProvider.h"

class ValveCoreModule : public IExtensionInterface,
                        public Core::INovaCapabilityProvider,
                        public Core::IValvePolicyProvider {
public:
    ValveCoreModule();
    ~ValveCoreModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    // INovaCapabilityProvider
    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;

    // IValvePolicyProvider
    Core::ValveLimitStatus CheckRequestLimit(const std::string& policyName, 
                                             const std::string& limitKey, 
                                             const std::string& contextJson) override;
    std::vector<std::string> ListActivePolicies() const override;
};
