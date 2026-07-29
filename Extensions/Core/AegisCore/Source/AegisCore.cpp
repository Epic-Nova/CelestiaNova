#include "AegisCore.h"

#include "Core/ExtensionRegistry.h"
#include "Core/NovaLog.h"
#include "ExtensionSpecific/IContentForge.h"
#include "KeyForgeDeploymentContracts.h"
#include <fstream>
#include <json.hpp>

namespace {
bool IsSafeHttpUrl(const std::string& value) {
    return (value.rfind("http://", 0) == 0 || value.rfind("https://", 0) == 0) &&
           value.find_first_of(" \t\r\n\"'`|&;<>") == std::string::npos;
}

struct AegisLoginContract {
    std::string authorizationServerId;
    std::string applicationId;
    std::vector<std::string> scopes;
    std::vector<std::string> audiences;
};

bool LoadContract(const std::string& contentId, AegisLoginContract& out, std::string& error) {
    auto* forge = dynamic_cast<Core::IContentForge*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("contentforge"));
    Core::LocalContentDescriptor content;
    if (!forge || !forge->ResolveLocalContent(contentId, content)) {
        error = "AegisCore could not resolve the selected content pack.";
        return false;
    }
    try {
        std::ifstream input(content.manifestPath);
        nlohmann::json manifest;
        input >> manifest;
        // `aegisLogin` is the v1 name.  The legacy key is read only so older
        // packs keep working while being migrated without reintroducing NovaID.
        const auto flow = manifest.contains("aegisLogin") ? manifest.at("aegisLogin") :
                          manifest.value("novaIdDeviceFlow", nlohmann::json::object());
        const auto app = flow.value("oauthApplication", nlohmann::json::object());
        out.authorizationServerId = app.value("authorizationServerId", "");
        out.applicationId = app.value("applicationId", "");
        out.scopes = app.value("scopes", std::vector<std::string>{});
        out.audiences = app.value("audiences", std::vector<std::string>{});
        if (flow.empty() || !IsSafeHttpUrl(flow.value("deviceAuthorizeUrl", "")) ||
            !IsSafeHttpUrl(flow.value("deviceApproveUrl", "")) ||
            !IsSafeHttpUrl(flow.value("deviceTokenUrl", "")) ||
            out.authorizationServerId.empty() || out.applicationId.empty() || out.scopes.empty()) {
            error = "The selected content pack has no complete, safe Aegis OAuth device-flow contract.";
            return false;
        }
        return true;
    } catch (...) {
        error = "The selected content pack has an invalid Aegis login declaration.";
        return false;
    }
}
} // namespace

AegisCoreModule::AegisCoreModule() = default;
AegisCoreModule::~AegisCoreModule() = default;

void AegisCoreModule::StartupModule() { NOVA_LOG("[AegisCore] StartupModule called", LogType::Log); }
void AegisCoreModule::ShutdownModule() { Logout(""); NOVA_LOG("[AegisCore] ShutdownModule called", LogType::Log); }

bool AegisCoreModule::BeginLogin(const std::string& contentId, std::string& outUrl, std::string& outError) {
    AegisLoginContract contract;
    if (!LoadContract(contentId, contract, outError)) return false;
    auto* keyForge = dynamic_cast<KeyForge::IDeploymentSecretBroker*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("keyforge"));
    if (!keyForge) { outError = "Aegis login requires KeyForge."; return false; }
    KeyForge::DeviceAuthorizationRequest request;
    request.requestorExtensionId = "aegiscore";
    request.applicationId = contract.applicationId;
    request.authorizationServerId = contract.authorizationServerId;
    request.scopes = contract.scopes;
    request.audiences = contract.audiences;
    const auto response = keyForge->BeginDeviceAuthorization(request);
    if (!response.accepted || response.deviceCode.empty() || !IsSafeHttpUrl(response.verificationUri)) {
        outError = "Aegis device login could not be created: " + response.receipt;
        return false;
    }
    std::lock_guard<std::mutex> lock(SessionMutex_);
    Session_ = {contentId, response.deviceCode, contract.applicationId, contract.authorizationServerId,
                response.verificationUri, "Waiting for approval", ""};
    outUrl = response.verificationUri;
    return true;
}

bool AegisCoreModule::PollLogin(const std::string& contentId, std::string& outStatus, std::string& outError) {
    SessionState session;
    { std::lock_guard<std::mutex> lock(SessionMutex_); session = Session_; }
    if (session.contentId != contentId || session.deviceCode.empty()) {
        outError = "Start an Aegis login before refreshing it."; return false;
    }
    auto* keyForge = dynamic_cast<KeyForge::IDeploymentSecretBroker*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("keyforge"));
    if (!keyForge) { outError = "Aegis login refresh requires KeyForge."; return false; }
    const auto response = keyForge->PollDeviceAuthorization(
        {"aegiscore", session.applicationId, session.authorizationServerId, session.deviceCode});
    if (response.pending) { outStatus = "Waiting for approval"; return true; }
    if (!response.accepted || response.accessToken.empty()) {
        outError = "Aegis device-token exchange failed: " + response.receipt; return false;
    }
    { std::lock_guard<std::mutex> lock(SessionMutex_); Session_.accessToken = response.accessToken; Session_.status = "Authenticated"; }
    outStatus = "Authenticated";
    return true;
}

void AegisCoreModule::Logout(const std::string&) { std::lock_guard<std::mutex> lock(SessionMutex_); Session_ = {}; }

bool AegisCoreModule::HasAuthenticatedAegisSession() const {
    std::lock_guard<std::mutex> lock(SessionMutex_); return !Session_.accessToken.empty();
}

bool AegisCoreModule::AuthorizeRemoteControlDispatch(const std::string& targetId, const std::string& capability,
    Core::RemoteControlDispatchAuthorization& out, std::string& error) const {
    out.authorizationHeader.clear();
    if (targetId.empty() || capability != "mesh.remote.execute") { error = "Remote dispatch requires a declared target and mesh.remote.execute."; return false; }
    std::lock_guard<std::mutex> lock(SessionMutex_);
    if (Session_.accessToken.empty()) { error = "An approved AegisCore session is required."; return false; }
    out.authorizationHeader = "Bearer " + Session_.accessToken;
    return true;
}

Core::CanvasMenuActionResult AegisCoreModule::OnMenuAction(const Core::CanvasMenuActionRequest& request) {
    Core::CanvasMenuActionResult result;
    if (request.ActionId.rfind("aegis.login.", 0) != 0) return result;
    const auto found = request.ContextValues.find("contentId");
    const std::string contentId = found == request.ContextValues.end() || found->second.empty() ? "auth-api" : found->second;
    if (request.ActionId == "aegis.login.begin") {
        std::string url; result.Success = BeginLogin(contentId, url, result.ErrorMessage);
        if (result.Success) { result.ConfigUpdates["aegisStatus"] = "Aegis: Waiting for approval"; result.ConfigUpdates["aegisLoginUrl"] = "Approval URL: " + url; }
    } else if (request.ActionId == "aegis.login.poll") {
        std::string status; result.Success = PollLogin(contentId, status, result.ErrorMessage);
        if (result.Success) result.ConfigUpdates["aegisStatus"] = "Aegis: " + status;
    } else if (request.ActionId == "aegis.login.logout") {
        Logout(contentId); result.ConfigUpdates["aegisStatus"] = "Aegis: Login Required"; result.ConfigUpdates["aegisLoginUrl"] = "Approval URL: Not generated";
    }
    return result;
}
