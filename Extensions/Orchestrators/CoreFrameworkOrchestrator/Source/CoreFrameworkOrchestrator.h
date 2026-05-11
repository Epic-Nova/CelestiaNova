#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"

#ifdef CoreFrameworkOrchestrator_EXPORTS
#  define COREFRAMEWORKORCHESTRATOR_API NOVA_EXPORT
#else
#  define COREFRAMEWORKORCHESTRATOR_API NOVA_IMPORT
#endif

#include <string>
#include <vector>
#include <map>

namespace CoreFramework {

struct FrameworkConfigPayload {
    std::string frameworkName;
    std::string contentForgeMountPath;
    std::string requestedDatabaseType;
    std::vector<int> requiredPorts;
};

struct FrameworkEnvironmentVars {
    std::map<std::string, std::string> envVars;
};

class ICoreFrameworkOrchestrator {
public:
    virtual ~ICoreFrameworkOrchestrator() = default;

    virtual FrameworkEnvironmentVars GenerateBaseEnvironment(const FrameworkConfigPayload& payload) const = 0;
    virtual std::string GetDefaultEntrypoint(const std::string& frameworkName) const = 0;
};

} // namespace CoreFramework

class COREFRAMEWORKORCHESTRATOR_API CoreFrameworkOrchestratorModule : 
    public IExtensionInterface, 
    public Core::INovaCapabilityProvider,
    public CoreFramework::ICoreFrameworkOrchestrator {
public:
    CoreFrameworkOrchestratorModule();
    ~CoreFrameworkOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;

    // ICoreFrameworkOrchestrator Implementation
    CoreFramework::FrameworkEnvironmentVars GenerateBaseEnvironment(const CoreFramework::FrameworkConfigPayload& payload) const override;
    std::string GetDefaultEntrypoint(const std::string& frameworkName) const override;
};
