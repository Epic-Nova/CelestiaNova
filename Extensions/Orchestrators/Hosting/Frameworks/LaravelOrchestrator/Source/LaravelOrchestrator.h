#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/IExtensionCliProvider.h"
#include "ExtensionSpecific/IMenuActionProvider.h"
#include <string>
#include <vector>

struct LaravelDeploymentResult {
    bool succeeded = false;
    std::string message;
};

#ifdef LaravelOrchestrator_EXPORTS
#  define LARAVELORCHESTRATOR_API NOVA_EXPORT
#else
#  define LARAVELORCHESTRATOR_API NOVA_IMPORT
#endif

class LARAVELORCHESTRATOR_API LaravelOrchestratorModule : public IExtensionInterface,
                                                           public Core::IExtensionCliProvider,
                                                           public Core::IMenuActionProvider {
public:
    LaravelOrchestratorModule();
    ~LaravelOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
    std::vector<Core::FExtensionCliArgDescriptor> GetCliArgDescriptors() const override;
    void ApplyCliArgs(const std::vector<Core::FExtensionCliArg>& args) override;
    Core::CanvasMenuActionResult OnMenuAction(const Core::CanvasMenuActionRequest& request) override;
    LaravelDeploymentResult DeployLocalContent(const std::string& contentId, const std::string& profile = "minimal") const;
};

#ifdef LaravelOrchestrator_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(LARAVELORCHESTRATOR_API, LaravelOrchestratorModule)
#endif

