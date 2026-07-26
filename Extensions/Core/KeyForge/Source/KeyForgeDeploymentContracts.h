#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace KeyForge {

// These types deliberately contain references and public metadata only. A
// resolved OAuth secret or environment value must never cross an extension ABI.
struct OAuthApplicationRequest {
    std::string requestorExtensionId;
    std::string applicationId;
    std::string authorizationServerId;
    std::vector<std::string> scopes;
    std::vector<std::string> audiences;
    std::uint32_t accessTokenTtlSeconds = 600;
};

struct OAuthApplicationLease {
    bool accepted = false;
    std::string applicationId;
    std::string clientIdReference;
    std::string clientSecretReference;
    std::string receipt;
};

// The device code is an opaque, short-lived correlation value. It may be kept
// in the active NovaID session only; it must never be persisted or logged.
struct DeviceAuthorizationRequest {
    std::string requestorExtensionId;
    std::string applicationId;
    std::string authorizationServerId;
    std::vector<std::string> scopes;
};

struct DeviceAuthorizationResponse {
    bool accepted = false;
    std::string verificationUri;
    std::string userCode;
    std::string deviceCode;
    std::uint32_t expiresInSeconds = 0;
    std::uint32_t pollingIntervalSeconds = 0;
    std::string receipt;
};

// Runtime materialization is an explicit deployment boundary. Public values
// may be rendered verbatim, while secret values are references owned by
// KeyForge. The destination must be a release-local .runtime.env path.
struct RuntimeEnvironmentRequest {
    std::string requestorExtensionId;
    std::string targetId;
    std::string remoteReleasePath;
    std::map<std::string, std::string> publicValues;
    std::map<std::string, std::string> secretReferences;
    std::string remoteWriteMode = "0600";
    bool removeAfterActivation = false;
};

struct RuntimeEnvironmentReceipt {
    bool accepted = false;
    std::string targetId;
    std::string remoteEnvironmentPath;
    std::string receipt;
};

class IDeploymentSecretBroker {
public:
    virtual ~IDeploymentSecretBroker() = default;

    // Provisions or retrieves the opaque client references for one Celestia
    // application. The Auth API client secret is never returned to callers.
    virtual OAuthApplicationLease EnsureOAuthApplication(const OAuthApplicationRequest& request) = 0;

    // Executes the secret-bearing request to the authorization server inside
    // KeyForge. Callers receive only a short-lived device-flow response.
    virtual DeviceAuthorizationResponse BeginDeviceAuthorization(
        const DeviceAuthorizationRequest& request) = 0;

    // Resolves secretReferences inside KeyForge and transfers a mode-0600
    // runtime environment directly to the authenticated target. Implementors
    // must not log content or return it in this receipt.
    virtual RuntimeEnvironmentReceipt MaterializeRemoteRuntimeEnvironment(
        const RuntimeEnvironmentRequest& request) = 0;
};

} // namespace KeyForge
