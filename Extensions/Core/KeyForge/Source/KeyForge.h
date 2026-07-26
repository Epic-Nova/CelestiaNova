#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "KeyForgeDeploymentContracts.h"
#include "KeyForgeEnvironmentHandoff.h"

#include <map>
#include <mutex>
#include <optional>

#ifdef KeyForge_EXPORTS
#  define KEYFORGE_API NOVA_EXPORT
#else
#  define KEYFORGE_API NOVA_IMPORT
#endif

class KEYFORGE_API KeyForgeModule : public IExtensionInterface,
                                   public KeyForge::IEnvironmentHandoff,
                                   public KeyForge::IDeploymentSecretBroker {
public:
    KeyForgeModule();
    ~KeyForgeModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    bool AcceptEnvironmentTargetHandoff(const std::string& requestorExtensionId,
                                        const std::string& environmentTarget,
                                        std::string& outReceipt) override;

    KeyForge::OAuthApplicationLease EnsureOAuthApplication(
        const KeyForge::OAuthApplicationRequest& request) override;
    KeyForge::DeviceAuthorizationResponse BeginDeviceAuthorization(
        const KeyForge::DeviceAuthorizationRequest& request) override;
    KeyForge::RuntimeEnvironmentReceipt MaterializeRemoteRuntimeEnvironment(
        const KeyForge::RuntimeEnvironmentRequest& request) override;

private:
    bool StoreSecret(const std::string& reference, const std::string& value);
    std::optional<std::string> ReadSecret(const std::string& reference) const;
    std::string VaultPath() const;
    std::mutex OAuthLeaseMutex_;
    std::map<std::string, KeyForge::OAuthApplicationLease> OAuthLeases_;
};

#ifdef KeyForge_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(KEYFORGE_API, KeyForgeModule)
#endif

