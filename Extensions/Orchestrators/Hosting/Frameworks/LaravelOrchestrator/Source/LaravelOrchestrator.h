#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/IExtensionCliProvider.h"
#include "ExtensionSpecific/IContentForge.h"
#include "ExtensionSpecific/IMenuActionProvider.h"
#include "ExtensionSpecific/IRemoteControl.h"
#include <map>
#include <mutex>
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
                                                           public Core::IMenuActionProvider,
                                                           public Core::INovaIdSessionCapabilityProvider {
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
    bool HasAuthenticatedNovaIdSession() const override;
    bool AuthorizeRemoteControlDispatch(const std::string& targetId,
                                        const std::string& requiredCapability,
                                        Core::RemoteControlDispatchAuthorization& outAuthorization,
                                        std::string& outError) const override;

private:
    struct NovaIdSessionState {
        std::string sessionId;
        std::string loginUrl;
        std::string status = "Login Required";
        // Access tokens are deliberately memory-only. They must never enter
        // Canvas config updates, menu values, content manifests, or logs.
        std::string accessToken;
    };

    std::string GetActiveReleasePath(const std::string& contentId) const;
    void RememberActiveRelease(const std::string& contentId, const std::string& releasePath);
    bool BeginNovaIdLogin(const Core::LocalContentDescriptor& content, std::string& outLoginUrl, std::string& outError);
    bool PollNovaIdLogin(const Core::LocalContentDescriptor& content, std::string& outStatus, std::string& outError);
    void LogoutNovaId(const std::string& contentId);
    bool HasNovaIdToken(const std::string& contentId) const;

    mutable std::mutex ActiveReleaseMutex_;
    std::map<std::string, std::string> ActiveReleasePaths_;
    mutable std::mutex NovaIdSessionMutex_;
    std::map<std::string, NovaIdSessionState> NovaIdSessions_;
};

#ifdef LaravelOrchestrator_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(LARAVELORCHESTRATOR_API, LaravelOrchestratorModule)
#endif

