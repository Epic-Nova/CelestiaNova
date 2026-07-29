#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/IExtensionCliProvider.h"
#include "ExtensionSpecific/IContentForge.h"
#include "ExtensionSpecific/IMenuActionProvider.h"
#include "ExtensionSpecific/IRemoteControl.h"
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
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
    LaravelDeploymentResult DeployLocalContent(const std::string& contentId, const std::string& profile = "auto");
    LaravelDeploymentResult DeployRemoteContent(const std::string& contentId, const std::string& profile = "auto");
private:
    std::string GetActiveReleasePath(const std::string& contentId) const;
    void RememberActiveRelease(const std::string& contentId, const std::string& releasePath);
    bool QueueLocalDeployment(const std::string& contentId, const std::string& profile, std::string& outJobId);

    mutable std::mutex ActiveReleaseMutex_;
    std::map<std::string, std::string> ActiveReleasePaths_;
    std::mutex DeploymentMutex_;
    std::set<std::string> QueuedDeployments_;
    std::vector<std::thread> DeploymentWorkers_;
};

#ifdef LaravelOrchestrator_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(LARAVELORCHESTRATOR_API, LaravelOrchestratorModule)
#endif

