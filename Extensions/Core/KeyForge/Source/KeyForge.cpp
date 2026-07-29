#include "KeyForge.h"

#include "Core/NovaLog.h"
#include "Core/ExtensionRegistry.h"
#include "IHTTPAgent.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <cstdlib>
#include <json.hpp>

#ifndef _WIN32
#include <sys/stat.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#include <winhttp.h>
#endif

namespace {

bool IsSafeIdentifier(const std::string& value) {
    return !value.empty() && value.size() <= 96 && std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::islower(character) || std::isdigit(character) || character == '.' || character == '_' || character == '-';
    });
}

bool IsSafeOAuthMetadata(const std::string& value) {
    return !value.empty() && value.size() <= 96 && std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::islower(character) || std::isdigit(character) || character == '.' || character == '_' ||
            character == '-' || character == ':';
    });
}

bool IsKeyForgeReference(const std::string& value) {
    constexpr const char* prefix = "keyforge://";
    if (value.rfind(prefix, 0) != 0 || value.size() <= std::string(prefix).size()) {
        return false;
    }
    return std::all_of(value.begin() + static_cast<std::string::difference_type>(std::string(prefix).size()), value.end(),
        [](unsigned char character) {
            return std::isalnum(character) || character == '.' || character == '_' || character == '-' || character == '/';
        });
}

bool IsSafeEnvironmentKey(const std::string& value) {
    return !value.empty() && value.size() <= 128 && std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isupper(character) || std::isdigit(character) || character == '_';
    });
}

bool IsSafeReleasePath(const std::string& value) {
    return value.rfind("/", 0) == 0 && value.find("..") == std::string::npos &&
        value.find_first_of("\r\n\0") == std::string::npos;
}

bool IsExplicitLocalTestAuthApiUrl(const std::string& value) {
    const auto* localTestMode = std::getenv("CELESTIA_LOCAL_TEST_MODE");
    const auto* configuredBase = std::getenv("CELESTIA_AUTH_API_BASE_URL");
    if (!localTestMode || std::string(localTestMode) != "1" || !configuredBase || !*configuredBase) return false;
    const std::string base(configuredBase);
    return base.rfind("http://", 0) == 0 && value.rfind(base + "/api/v1/", 0) == 0 &&
        value.find_first_of("\r\n") == std::string::npos;
}

bool IsAllowedOAuthEndpoint(const std::string& value) {
    return value.rfind("https://", 0) == 0 || IsExplicitLocalTestAuthApiUrl(value);
}

std::string ReferenceKey(const std::string& reference) {
    return reference.substr(std::string("keyforge://").size());
}

std::string CredentialFileName(const std::string& reference) {
    std::string name = ReferenceKey(reference);
    for (char& character : name) {
        if (character == '/') character = '-';
    }
    return name;
}

// The vault file is a sequence of DPAPI-protected values.  Names remain only
// as DPAPI-protected data too, so an offline reader cannot enumerate secret
// identities.  The user/machine binding is intentionally CurrentUser.
#ifdef _WIN32
bool Protect(const std::string& plain, std::vector<unsigned char>& protectedValue) {
    DATA_BLOB input{static_cast<DWORD>(plain.size()), reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()))};
    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"CelestiaNova.KeyForge.v1", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output)) return false;
    protectedValue.assign(output.pbData, output.pbData + output.cbData);
    LocalFree(output.pbData);
    return true;
}
bool Unprotect(const std::vector<unsigned char>& encrypted, std::string& plain) {
    DATA_BLOB input{static_cast<DWORD>(encrypted.size()), const_cast<BYTE*>(encrypted.data())};
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) return false;
    plain.assign(reinterpret_cast<const char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    return true;
}
void WriteU32(std::ostream& out, std::uint32_t value) { out.write(reinterpret_cast<const char*>(&value), sizeof(value)); }
bool ReadU32(std::istream& in, std::uint32_t& value) { return static_cast<bool>(in.read(reinterpret_cast<char*>(&value), sizeof(value))); }

std::optional<std::string> JsonString(const std::string& json, const std::string& key) {
    const auto needle = "\"" + key + "\"";
    const auto at = json.find(needle);
    if (at == std::string::npos) return std::nullopt;
    const auto colon = json.find(':', at + needle.size());
    const auto first = json.find('"', colon == std::string::npos ? at : colon);
    if (first == std::string::npos) return std::nullopt;
    const auto last = json.find('"', first + 1);
    if (last == std::string::npos || json.find('\\', first + 1) < last) return std::nullopt;
    return json.substr(first + 1, last - first - 1);
}

std::optional<std::string> OutputValue(const std::string& output, const std::string& label) {
    if (const auto json = JsonString(output, label); json) return json;
    const std::string humanLabel = label == "client_id" ? "Client ID:" : "Client Secret:";
    const auto at = output.find(humanLabel); if (at == std::string::npos) return std::nullopt;
    const auto begin = output.find_first_not_of(" \t", at + humanLabel.size());
    const auto end = output.find_first_of("\r\n", begin);
    return begin == std::string::npos ? std::nullopt : std::optional<std::string>(output.substr(begin, end - begin));
}

std::optional<unsigned short> JsonPort(const std::string& json) {
    const auto at=json.find("\"port\""); if (at==std::string::npos) return std::nullopt;
    const auto colon=json.find(':',at); if (colon==std::string::npos) return std::nullopt;
    const auto first=json.find_first_of("0123456789",colon); if(first==std::string::npos) return std::nullopt;
    const auto last=json.find_first_not_of("0123456789",first); const auto value=std::stoul(json.substr(first,last-first));
    return value>0 && value<=65535 ? std::optional<unsigned short>(static_cast<unsigned short>(value)) : std::nullopt;
}

std::optional<std::string> HttpPostForm(const std::string& url, const std::string& form, const std::string& additionalHeaders) {
    const bool isHttps = url.rfind("https://", 0) == 0;
    // The only plaintext exception is the exact endpoint explicitly selected
    // by a local test operator. Production never enables this switch.
    if (!isHttps && !IsExplicitLocalTestAuthApiUrl(url)) return std::nullopt;
    URL_COMPONENTSW parts{}; parts.dwStructSize = sizeof(parts); parts.dwSchemeLength = parts.dwHostNameLength = parts.dwUrlPathLength = static_cast<DWORD>(-1);
    const std::wstring wideUrl(url.begin(), url.end());
    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &parts)) return std::nullopt;
    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    const std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    HINTERNET session = WinHttpOpen(L"CelestiaNova-KeyForge/1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!session) return std::nullopt;
    HINTERNET connection = WinHttpConnect(session, host.c_str(), parts.nPort, 0);
    HINTERNET request = connection ? WinHttpOpenRequest(connection, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, isHttps ? WINHTTP_FLAG_SECURE : 0) : nullptr;
    std::wstring headers = L"Content-Type: application/x-www-form-urlencoded\r\n";
    if (!additionalHeaders.empty()) headers += std::wstring(additionalHeaders.begin(), additionalHeaders.end());
    BOOL sent = request && WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(headers.size()), const_cast<char*>(form.data()), static_cast<DWORD>(form.size()), static_cast<DWORD>(form.size()), 0) && WinHttpReceiveResponse(request, nullptr);
    std::string response;
    while (sent) { DWORD available = 0; if (!WinHttpQueryDataAvailable(request, &available) || !available) break; const auto old = response.size(); response.resize(old + available); DWORD received = 0; if (!WinHttpReadData(request, response.data() + old, available, &received)) { sent = FALSE; break; } response.resize(old + received); }
    if (request) WinHttpCloseHandle(request); if (connection) WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
    return sent ? std::optional<std::string>(response) : std::nullopt;
}

bool WriteRemoteEnvironment(const std::string& targetId, const std::string& remotePath, const std::string& contents) {
    const auto targetsRoot = std::filesystem::current_path() / "Content" / "RemoteTargets";
    std::string target;
    for (const auto& candidate : std::filesystem::directory_iterator(targetsRoot)) {
        if (!candidate.is_regular_file() || candidate.path().extension() != ".json") continue;
        std::ifstream targetFile(candidate.path()); std::string parsed((std::istreambuf_iterator<char>(targetFile)), {});
        if (JsonString(parsed,"id").value_or("") == targetId) { target=std::move(parsed); break; }
    }
    const auto host = JsonString(target, "host"); const auto user = JsonString(target, "user");
    const auto knownHosts = JsonString(target, "knownHostsFile");
    const auto port=JsonPort(target);
    if (!host || !user || !knownHosts || !port || !IsSafeIdentifier(targetId) || remotePath.find('\'') != std::string::npos) return false;
    const auto knownPath = (std::filesystem::current_path() / *knownHosts).string();
    if (!std::filesystem::is_regular_file(knownPath)) return false;
    // Secret bytes travel only through the child stdin pipe. They are absent
    // from its command line, console output, environment and Nova logs.
    std::string command = "ssh.exe -o BatchMode=yes -o StrictHostKeyChecking=yes -o UserKnownHostsFile=\"" + knownPath +
        "\" -p " + std::to_string(*port) + " \"" + *user + "@" + *host + "\" \"umask 077; cat > '" + remotePath + "' && chmod 600 '" + remotePath + "'\"";
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, TRUE}; HANDLE readPipe=nullptr, writePipe=nullptr;
    if (!CreatePipe(&readPipe,&writePipe,&attributes,0)) return false;
    SetHandleInformation(writePipe, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA startup{}; startup.cb=sizeof(startup); startup.dwFlags=STARTF_USESTDHANDLES; startup.hStdInput=readPipe; startup.hStdOutput=GetStdHandle(STD_OUTPUT_HANDLE); startup.hStdError=GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process{}; std::vector<char> mutableCommand(command.begin(),command.end()); mutableCommand.push_back('\0');
    const BOOL created=CreateProcessA(nullptr,mutableCommand.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process);
    CloseHandle(readPipe); if (!created) { CloseHandle(writePipe); return false; }
    DWORD written=0; const BOOL writtenOk=WriteFile(writePipe,contents.data(),static_cast<DWORD>(contents.size()),&written,nullptr); CloseHandle(writePipe);
    WaitForSingleObject(process.hProcess, 30000); DWORD exitCode=1; GetExitCodeProcess(process.hProcess,&exitCode); CloseHandle(process.hThread); CloseHandle(process.hProcess);
    return writtenOk && written==contents.size() && exitCode==0;
}
#endif

} // namespace

KeyForgeModule::KeyForgeModule() {}
KeyForgeModule::~KeyForgeModule() {}

void KeyForgeModule::StartupModule() {
#ifdef _WIN32
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(VaultPath()).parent_path(), error);
    NOVA_LOG(error ? "[KeyForge] DPAPI vault directory unavailable; vault is fail-closed." : "[KeyForge] Windows DPAPI vault backend ready.", error ? LogType::Warning : LogType::Log);
#else
    const char* credentialDirectory = std::getenv("CREDENTIALS_DIRECTORY");
    NOVA_LOG(credentialDirectory && credentialDirectory[0]
        ? "[KeyForge] Linux systemd credential vault backend ready."
        : "[KeyForge] Linux credential directory is absent; vault is fail-closed.",
        credentialDirectory && credentialDirectory[0] ? LogType::Log : LogType::Warning);
#endif
}

void KeyForgeModule::ShutdownModule() {
    NOVA_LOG("[KeyForge] ShutdownModule called", LogType::Log);
}

bool KeyForgeModule::AcceptEnvironmentTargetHandoff(const std::string& requestorExtensionId,
                                                    const std::string& environmentTarget,
                                                    std::string& outReceipt) {
    if (environmentTarget.empty()) {
        outReceipt = "rejected: empty environment target";
        NOVA_LOG("[KeyForge] Example handoff rejected (empty environment target)", LogType::Warning);
        return false;
    }

    std::ostringstream message;
    message << "[KeyForge] Example handoff accepted from '" << requestorExtensionId
            << "' for environment target '" << environmentTarget << "'";
    NOVA_LOG(message.str().c_str(), LogType::Log);

    outReceipt = "accepted: " + requestorExtensionId + " -> " + environmentTarget;
    return true;
}

std::string KeyForgeModule::VaultPath() const {
    return (std::filesystem::current_path() / "Content" / ".runtime" / "keyforge-v1.dpapi").string();
}

bool KeyForgeModule::StoreSecret(const std::string& reference, const std::string& value) {
#ifndef _WIN32
    (void)reference; (void)value; return false;
#else
    if (!IsKeyForgeReference(reference) || value.empty() || value.find('\0') != std::string::npos) return false;
    std::map<std::string, std::string> entries;
    if (std::ifstream input(VaultPath(), std::ios::binary); input) {
        char magic[8]{}; input.read(magic, sizeof(magic)); std::uint32_t count = 0;
        if (std::string(magic, sizeof(magic)) != "KFV1DPAP" || !ReadU32(input, count) || count > 4096) return false;
        for (std::uint32_t i = 0; i < count; ++i) { std::uint32_t keySize=0, valueSize=0; if (!ReadU32(input,keySize) || keySize>65536) return false; std::vector<unsigned char> key(keySize); if (!input.read(reinterpret_cast<char*>(key.data()),keySize) || !ReadU32(input,valueSize) || valueSize>1048576) return false; std::vector<unsigned char> secret(valueSize); if (!input.read(reinterpret_cast<char*>(secret.data()),valueSize)) return false; std::string decodedKey, decodedValue; if (!Unprotect(key,decodedKey)||!Unprotect(secret,decodedValue)) return false; entries.emplace(std::move(decodedKey),std::move(decodedValue)); }
    }
    entries[ReferenceKey(reference)] = value;
    const auto temporary = VaultPath() + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc); if (!output) return false;
    output.write("KFV1DPAP",8); WriteU32(output,static_cast<std::uint32_t>(entries.size()));
    for (const auto& entry : entries) { std::vector<unsigned char> key, secret; if (!Protect(entry.first,key)||!Protect(entry.second,secret)) return false; WriteU32(output,static_cast<std::uint32_t>(key.size())); output.write(reinterpret_cast<const char*>(key.data()),key.size()); WriteU32(output,static_cast<std::uint32_t>(secret.size())); output.write(reinterpret_cast<const char*>(secret.data()),secret.size()); }
    output.close(); if (!output) return false;
    return MoveFileExA(temporary.c_str(), VaultPath().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
#endif
}

std::optional<std::string> KeyForgeModule::ReadSecret(const std::string& reference) const {
#ifndef _WIN32
    if (!IsKeyForgeReference(reference)) return std::nullopt;
    const char* credentialDirectory = std::getenv("CREDENTIALS_DIRECTORY");
    if (!credentialDirectory || credentialDirectory[0] == '\0') return std::nullopt;
    // systemd credential names are flat identifiers; reference slashes were
    // normalized to dashes by CredentialFileName before encryption.
    const auto path = std::filesystem::path(credentialDirectory) / ("keyforge_" + CredentialFileName(reference));
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::nullopt;
    std::string value((std::istreambuf_iterator<char>(input)), {});
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) value.pop_back();
    return value.empty() || value.find('\0') != std::string::npos ? std::nullopt : std::optional<std::string>(std::move(value));
#else
    if (!IsKeyForgeReference(reference)) return std::nullopt;
    std::ifstream input(VaultPath(), std::ios::binary); char magic[8]{}; std::uint32_t count=0;
    if (!input.read(magic,8) || std::string(magic,8)!="KFV1DPAP" || !ReadU32(input,count) || count>4096) return std::nullopt;
    for (std::uint32_t i=0;i<count;++i) { std::uint32_t ks=0,vs=0; if(!ReadU32(input,ks)||ks>65536) return std::nullopt; std::vector<unsigned char> key(ks); if(!input.read(reinterpret_cast<char*>(key.data()),ks)||!ReadU32(input,vs)||vs>1048576) return std::nullopt; std::vector<unsigned char> value(vs); if(!input.read(reinterpret_cast<char*>(value.data()),vs)) return std::nullopt; std::string k,v; if(!Unprotect(key,k)||!Unprotect(value,v)) return std::nullopt; if(k==ReferenceKey(reference)) return v; }
    return std::nullopt;
#endif
}

bool KeyForgeModule::DispatchOAuthAuthenticatedRequest(const KeyForge::OAuthAuthenticatedRequest& request,
    std::function<void(KeyForge::OAuthAuthenticatedResponse)> onComplete) {
    const auto complete = [callback = std::move(onComplete)](KeyForge::OAuthAuthenticatedResponse response) {
        if (callback) callback(std::move(response));
    };
    if (!IsAllowedOAuthEndpoint(request.tokenEndpoint) || !IsAllowedOAuthEndpoint(request.resourceUrl) || request.application.applicationId.empty()) {
        complete({false, 0, {}, "rejected: invalid OAuth endpoints or application"}); return false;
    }
    const auto lease = EnsureOAuthApplication(request.application);
    const auto clientId = lease.accepted ? ReadSecret(lease.clientIdReference) : std::nullopt;
    const auto clientSecret = lease.accepted ? ReadSecret(lease.clientSecretReference) : std::nullopt;
    auto* http = dynamic_cast<Core::IHTTPAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("httpagent"));
    if (!clientId || !clientSecret || !http) { complete({false, 0, {}, "rejected: protected OAuth lease or HTTPAgent unavailable"}); return false; }
    const auto encode = [](const std::string& value) { static constexpr char hex[] = "0123456789ABCDEF"; std::string out; for (const unsigned char c : value) { if (std::isalnum(c) || c == '-' || c == '_' || c == '.') out += static_cast<char>(c); else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; } } return out; };
    std::string scopes; for (const auto& scope : request.application.scopes) { if (!scopes.empty()) scopes += ' '; scopes += scope; }
    Core::SecureHttpsRequest token; token.url = request.tokenEndpoint; token.method = "POST";
    token.body = "grant_type=client_credentials&client_id=" + encode(*clientId) + "&client_secret=" + encode(*clientSecret) + "&scope=" + encode(scopes);
    token.headers.emplace("Content-Type", "application/x-www-form-urlencoded"); token.headers.emplace("Accept", "application/json");
    const auto dispatchId = http->DispatchSecureHttpsAsync(token, [http, request, complete = std::move(complete)](Core::SecureHttpsResponse tokenResponse) mutable {
        try {
            const auto accessToken = nlohmann::json::parse(tokenResponse.body).value("access_token", "");
            if (!tokenResponse.transportSucceeded || tokenResponse.statusCode < 200 || tokenResponse.statusCode >= 300 || accessToken.empty()) { complete({false, tokenResponse.statusCode, {}, "rejected: OAuth token request failed"}); return; }
            Core::SecureHttpsRequest resource; resource.url = request.resourceUrl; resource.method = request.method; resource.body = request.body; resource.headers = request.headers; resource.headers["Authorization"] = "Bearer " + accessToken; resource.headers.emplace("Accept", "application/json");
            http->DispatchSecureHttpsAsync(resource, [complete = std::move(complete)](Core::SecureHttpsResponse response) mutable { complete({response.transportSucceeded && response.statusCode >= 200 && response.statusCode < 300, response.statusCode, std::move(response.body), response.transportSucceeded ? "completed" : "transport failed"}); });
        } catch (...) { complete({false, tokenResponse.statusCode, {}, "rejected: invalid OAuth token response"}); }
    });
    return !dispatchId.empty();
}

KeyForge::OAuthApplicationLease KeyForgeModule::EnsureOAuthApplication(
    const KeyForge::OAuthApplicationRequest& request) {
    KeyForge::OAuthApplicationLease lease;
    lease.applicationId = request.applicationId;
    if (!IsSafeIdentifier(request.requestorExtensionId) || !IsSafeIdentifier(request.applicationId) ||
        !IsSafeIdentifier(request.authorizationServerId) || request.scopes.empty() ||
        request.accessTokenTtlSeconds < 60 || request.accessTokenTtlSeconds > 3600) {
        lease.receipt = "rejected: invalid OAuth application request";
        NOVA_LOG("[KeyForge] Rejected invalid OAuth application request.", LogType::Warning);
        return lease;
    }
    if (!std::all_of(request.scopes.begin(), request.scopes.end(), IsSafeOAuthMetadata) ||
        !std::all_of(request.audiences.begin(), request.audiences.end(), IsSafeOAuthMetadata)) {
        lease.receipt = "rejected: invalid OAuth scope or audience";
        NOVA_LOG("[KeyForge] Rejected invalid OAuth scope or audience metadata.", LogType::Warning);
        return lease;
    }

    const auto leaseKey = request.authorizationServerId + ":" + request.applicationId;
    std::lock_guard<std::mutex> lock(OAuthLeaseMutex_);
    const auto existing = OAuthLeases_.find(leaseKey);
    if (existing != OAuthLeases_.end()) {
        return existing->second;
    }

    const auto referenceRoot = "keyforge://oauth/" + request.authorizationServerId + "/" + request.applicationId;
    lease.clientIdReference = referenceRoot + "/client-id";
    lease.clientSecretReference = referenceRoot + "/client-secret";
    // Existing vault material is sufficient and never leaves KeyForge.
    if (ReadSecret(lease.clientIdReference).has_value() && ReadSecret(lease.clientSecretReference).has_value()) {
        lease.accepted = true;
        lease.receipt = "accepted: existing OAuth application credential lease";
        OAuthLeases_.emplace(leaseKey, lease);
        return lease;
    }

#ifdef _WIN32
    // Auth API deliberately exposes registration as an operator command, not
    // a public HTTP endpoint.  The configurable local command is therefore
    // the only provisioning bridge. It contains no secret and its one-time
    // secret output is captured in memory, parsed, DPAPI-protected, then
    // discarded without reaching a log or UI surface.
    const auto configPath = std::filesystem::current_path() / "Configs" / "KeyForge" / "LocalVault.json";
    std::ifstream configFile(configPath); std::string config((std::istreambuf_iterator<char>(configFile)), {});
    const auto provisioningEndpoint = JsonString(config, "authApiProvisionEndpoint");
    const auto bootstrapReference = JsonString(config, "bootstrapSecretReference");
    if (provisioningEndpoint && bootstrapReference && IsKeyForgeReference(*bootstrapReference)) {
        const auto bootstrapSecret = ReadSecret(*bootstrapReference);
        if (bootstrapSecret) {
            std::string scopes; for (const auto& scope : request.scopes) { if (!scopes.empty()) scopes += "%20"; scopes += scope; }
            std::string audiences; for (const auto& audience : request.audiences) { if (!audiences.empty()) audiences += "%20"; audiences += audience; }
            const auto body = "application_id=" + request.applicationId + "&scopes=" + scopes + "&audiences=" + audiences +
                "&access_token_ttl_seconds=" + std::to_string(request.accessTokenTtlSeconds);
            const auto payload = HttpPostForm(*provisioningEndpoint, body, "X-KeyForge-Bootstrap: " + *bootstrapSecret + "\r\n");
            if (payload) {
                const auto clientId = OutputValue(*payload, "client_id");
                const auto clientSecret = OutputValue(*payload, "client_secret");
                if (clientId && clientSecret && StoreSecret(lease.clientIdReference, *clientId) &&
                    StoreSecret(lease.clientSecretReference, *clientSecret)) {
                    lease.accepted = true;
                    lease.receipt = "accepted: OAuth application credential provisioned";
                    OAuthLeases_.emplace(leaseKey, lease);
                    return lease;
                }
            }
        }
    }

    // Compatibility fallback for installations that do not expose the
    // bootstrap-protected endpoint yet. It is deliberately secondary; new
    // configurations should use authApiProvisionEndpoint above.
    const auto templateCommand = JsonString(config, "authApiRegisterCommand");
    if (templateCommand && !templateCommand->empty() && templateCommand->find('\r') == std::string::npos && templateCommand->find('\n') == std::string::npos) {
        std::string scopes;
        for (const auto& scope : request.scopes) { if (!scopes.empty()) scopes += " "; scopes += scope; }
        std::string command = *templateCommand;
        const auto replace = [&command](const std::string& needle, const std::string& value) { size_t p=0; while ((p=command.find(needle,p))!=std::string::npos) { command.replace(p,needle.size(),value); p+=value.size(); } };
        replace("{application}", request.applicationId); replace("{scopes}", scopes); replace("{ttl}", std::to_string(request.accessTokenTtlSeconds));
        FILE* pipe = _popen(command.c_str(), "r"); std::string output; char buffer[256];
        if (pipe) { while (fgets(buffer, sizeof(buffer), pipe)) output += buffer; const int status = _pclose(pipe); if (status == 0) { const auto clientId = OutputValue(output, "client_id"); const auto clientSecret = OutputValue(output, "client_secret"); if (clientId && clientSecret && StoreSecret(lease.clientIdReference,*clientId) && StoreSecret(lease.clientSecretReference,*clientSecret)) { lease.accepted=true; lease.receipt="accepted: OAuth application credential provisioned"; OAuthLeases_.emplace(leaseKey,lease); return lease; } } }
    }
#endif
    lease.receipt = "rejected: OAuth credentials are absent and no local KeyForge provisioning command succeeded";
    NOVA_LOG("[KeyForge] OAuth application provisioning failed without exposing credential material.", LogType::Warning);
    return lease;
}

KeyForge::DeviceAuthorizationResponse KeyForgeModule::BeginDeviceAuthorization(
    const KeyForge::DeviceAuthorizationRequest& request) {
    KeyForge::DeviceAuthorizationResponse response;
    if (!IsSafeIdentifier(request.requestorExtensionId) || !IsSafeIdentifier(request.applicationId) ||
        !IsSafeIdentifier(request.authorizationServerId) || request.scopes.empty() ||
        !std::all_of(request.scopes.begin(), request.scopes.end(), IsSafeOAuthMetadata)) {
        response.receipt = "rejected: invalid device authorization request";
        NOVA_LOG("[KeyForge] Rejected invalid device authorization request.", LogType::Warning);
        return response;
    }

    const auto root = "keyforge://oauth/" + request.authorizationServerId + "/" + request.applicationId;
    const auto clientId = ReadSecret(root + "/client-id");
    const auto clientSecret = ReadSecret(root + "/client-secret");
#ifdef _WIN32
    const auto configPath = std::filesystem::current_path() / "Configs" / "KeyForge" / "LocalVault.json";
    std::ifstream configFile(configPath); std::string config((std::istreambuf_iterator<char>(configFile)), {});
    const auto endpoint = JsonString(config, "deviceAuthorizationEndpoint");
    if (clientId && clientSecret && endpoint) {
        std::string scope; for (const auto& value : request.scopes) { if (!scope.empty()) scope += "%20"; scope += value; }
        // Auth API credentials are sent exclusively in the TLS request body.
        const auto body = "client_id=" + *clientId + "&client_secret=" + *clientSecret + "&scope=" + scope;
        const auto payload = HttpPostForm(*endpoint, body, "");
        if (payload) { const auto uri=JsonString(*payload,"verification_uri"); const auto code=JsonString(*payload,"user_code"); const auto device=JsonString(*payload,"device_code"); if (uri&&code&&device) { response.accepted=true; response.verificationUri=*uri; response.userCode=*code; response.deviceCode=*device; response.expiresInSeconds=600; response.pollingIntervalSeconds=5; response.receipt="accepted: device authorization started"; return response; } }
    }
#endif
    response.receipt = "rejected: missing protected OAuth lease or secure device authorization endpoint";
    NOVA_LOG("[KeyForge] Device authorization rejected without exposing credential material.", LogType::Warning);
    return response;
}

KeyForge::RuntimeEnvironmentReceipt KeyForgeModule::MaterializeRemoteRuntimeEnvironment(
    const KeyForge::RuntimeEnvironmentRequest& request) {
    KeyForge::RuntimeEnvironmentReceipt receipt;
    receipt.targetId = request.targetId;
    receipt.remoteEnvironmentPath = request.remoteReleasePath + "/.runtime.env";
    if (!IsSafeIdentifier(request.requestorExtensionId) || !IsSafeIdentifier(request.targetId) ||
        !IsSafeReleasePath(request.remoteReleasePath) || request.secretReferences.empty() || request.remoteWriteMode != "0600") {
        receipt.receipt = "rejected: invalid runtime environment request";
        NOVA_LOG("[KeyForge] Rejected invalid runtime environment request.", LogType::Warning);
        return receipt;
    }
    for (const auto& entry : request.publicValues) {
        if (!IsSafeEnvironmentKey(entry.first) || entry.second.find_first_of("\r\n\0") != std::string::npos) {
            receipt.receipt = "rejected: invalid public environment value";
            NOVA_LOG("[KeyForge] Rejected invalid public environment metadata.", LogType::Warning);
            return receipt;
        }
    }
    for (const auto& entry : request.secretReferences) {
        if (!IsSafeEnvironmentKey(entry.first) || !IsKeyForgeReference(entry.second)) {
            receipt.receipt = "rejected: invalid secret reference";
            NOVA_LOG("[KeyForge] Rejected invalid runtime secret reference.", LogType::Warning);
            return receipt;
        }
    }

    std::string content;
    for (const auto& entry : request.publicValues) content += entry.first + "=" + entry.second + "\n";
    for (const auto& entry : request.secretReferences) {
        const auto secret = ReadSecret(entry.second);
        if (!secret || secret->find_first_of("\r\n\0") != std::string::npos) {
            receipt.receipt = "rejected: referenced secret is absent or unsafe";
            NOVA_LOG("[KeyForge] Runtime environment materialization rejected; secret was not exposed.", LogType::Warning);
            return receipt;
        }
        content += entry.first + "=" + *secret + "\n";
    }
#ifndef _WIN32
    if (request.targetId == "local-service") {
        const auto destination = std::filesystem::path(request.remoteReleasePath) / ".env";
        receipt.remoteEnvironmentPath = destination.string();
        std::error_code error;
        std::filesystem::create_directories(destination.parent_path(), error);
        if (!error) {
            std::ofstream output(destination, std::ios::binary | std::ios::trunc);
            if (output) {
                output.write(content.data(), static_cast<std::streamsize>(content.size()));
                output.close();
                chmod(destination.c_str(), S_IRUSR | S_IWUSR);
                receipt.accepted = static_cast<bool>(output);
                receipt.receipt = receipt.accepted ? "accepted: protected local runtime environment written" : "rejected: local runtime environment write failed";
                return receipt;
            }
        }
        receipt.receipt = "rejected: protected local runtime environment write failed";
        return receipt;
    }
#endif
#ifdef _WIN32
    if (WriteRemoteEnvironment(request.targetId, receipt.remoteEnvironmentPath, content)) {
        receipt.accepted = true; receipt.receipt = "accepted: protected runtime environment written";
        NOVA_LOG("[KeyForge] Protected runtime environment written to verified remote target.", LogType::Log);
        return receipt;
    }
#endif
    receipt.receipt = "rejected: protected remote writer unavailable or remote verification failed";
    NOVA_LOG("[KeyForge] Runtime environment writer failed closed.", LogType::Warning);
    return receipt;
}
