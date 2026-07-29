#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/IMenuActionProvider.h"
#include "ExtensionSpecific/IRemoteControl.h"
#include <mutex>
#include <string>

#ifdef AegisCore_EXPORTS
#  define AEGISCORE_API NOVA_EXPORT
#else
#  define AEGISCORE_API NOVA_IMPORT
#endif

class AEGISCORE_API AegisCoreModule : public IExtensionInterface,
                                      public Core::IMenuActionProvider,
                                      public Core::IAegisSessionCapabilityProvider {
public:
    AegisCoreModule();
    ~AegisCoreModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
    Core::CanvasMenuActionResult OnMenuAction(const Core::CanvasMenuActionRequest& request) override;
    bool HasAuthenticatedAegisSession() const override;
    bool AuthorizeRemoteControlDispatch(const std::string& targetId,
                                        const std::string& requiredCapability,
                                        Core::RemoteControlDispatchAuthorization& outAuthorization,
                                        std::string& outError) const override;

private:
    struct SessionState {
        std::string contentId;
        std::string deviceCode;
        std::string userCode;
        std::string applicationId;
        std::string authorizationServerId;
        std::string verificationUri;
        std::string status = "Login Required";
        std::string accessToken;
    };

    bool BeginLogin(const std::string& contentId, std::string& outUrl, std::string& outError);
    bool PollLogin(const std::string& contentId, std::string& outStatus, std::string& outError);
    bool ApproveLocalBypass(const Core::CanvasMenuActionRequest& request, std::string& outError);
    void Logout(const std::string& contentId);
    mutable std::mutex SessionMutex_;
    SessionState Session_;
};

#ifdef AegisCore_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(AEGISCORE_API, AegisCoreModule)
#endif

