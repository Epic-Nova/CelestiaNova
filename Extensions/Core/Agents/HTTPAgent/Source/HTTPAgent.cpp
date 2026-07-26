#include "HTTPAgent.h"
#include "Core/ExtensionRegistry.h"
#include "Core/NovaLog.h"
#include "TerminalAgent.h"
#include "ExtensionSpecific/IPackageManagerAgent.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <atomic>
#include <algorithm>

#if defined(_WIN32)
#  include <windows.h>
#  include <winhttp.h>
#  pragma comment(lib, "winhttp.lib")
#endif

namespace Core {

HTTPAgentModule::HTTPAgentModule() {}
HTTPAgentModule::~HTTPAgentModule() {}

void HTTPAgentModule::StartupModule() {
    NOVA_LOG("[HTTPAgent] StartupModule called. Network connectivity ready.", LogType::Log);
}

void HTTPAgentModule::ShutdownModule() {
    NOVA_LOG("[HTTPAgent] ShutdownModule called.", LogType::Log);
}

void HTTPAgentModule::AddLog(const std::string& message) {
    std::lock_guard<std::mutex> lock(ProgressMutex_);
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_struct;
#if defined(_WIN32)
    localtime_s(&tm_struct, &time);
#else
    localtime_r(&time, &tm_struct);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_struct, "%H:%M:%S");
    
    Logs_.push_back({oss.str(), message});
    if (Logs_.size() > 500) {
        Logs_.erase(Logs_.begin());
    }
}

CanvasMenuActionResult HTTPAgentModule::OnMenuAction(const CanvasMenuActionRequest& request) {
    CanvasMenuActionResult result;
    result.Success = true;

    if (request.ActionId == "http.action.test_connectivity") {
        std::string url = "https://google.com";
        std::map<std::string, std::string> headers;

        auto urlIt = request.ContextValues.find("http_url");
        if (urlIt != request.ContextValues.end() && !urlIt->second.empty()) {
            url = urlIt->second;
        }

        auto headersIt = request.ContextValues.find("http_headers");
        if (headersIt != request.ContextValues.end()) {
            std::istringstream stream(headersIt->second);
            std::string line;
            while (std::getline(stream, line)) {
                auto pos = line.find(':');
                if (pos == std::string::npos) continue;
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                if (!key.empty()) {
                    headers[key] = value;
                }
            }
        }

        std::thread([this, url, headers]() {
            Get(url, headers);
        }).detach();
        result.NavigateToMenuId = "httpagent::http_progress";
    }

    return result;
}

Core::RequirementResolver::CoreRequirementResolveResult HTTPAgentModule::Resolve(const Core::RequirementResolver::CoreRequirementResolveRequest& request) {
    Core::RequirementResolver::CoreRequirementResolveResult result;
    result.Success = true;

    if (request.RequirementKey == "http.logs") {
        std::string allLogs;
        std::lock_guard<std::mutex> lock(ProgressMutex_);
        for (const auto& log : Logs_) {
            allLogs += "[" + log.Timestamp + "] " + log.Message + "\n";
        }
        Core::RequirementResolver::CoreRequirementResolvedOption option;
        option.Value = allLogs;
        result.Options.push_back(option);
    } else {
        result.Success = false;
    }

    return result;
}

bool HTTPAgentModule::IsInstalled() const {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return false;

    CoreTerminal::TerminalCommandRequest req;
    req.command = "curl --version";
    auto result = terminalAgent->ExecuteCommandSync(req);
    return result.exitCode == 0;
}

bool HTTPAgentModule::Install(std::function<void(const std::string&)> onProgress) {
    auto pmAgent = dynamic_cast<IPackageManagerAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("packagemanageragent"));
    if (!pmAgent) return false;

    AddLog("Requesting curl installation via PackageManagerAgent...");
    return pmAgent->InstallPackage("curl");
}

bool HTTPAgentModule::Uninstall() {
    return true;
}

bool HTTPAgentModule::RunCommand(const std::string& command, std::string& outOutput) {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return false;

    CoreTerminal::TerminalCommandRequest req;
    req.command = "curl " + command;
    auto result = terminalAgent->ExecuteCommandSync(req);
    
    outOutput = result.stdOut;
    AddLog("HTTP CMD: curl " + command);
    return result.exitCode == 0;
}

bool HTTPAgentModule::Configure(const std::string& configKey, const std::string& configValue) {
    return true;
}

std::string HTTPAgentModule::Get(const std::string& url, const std::map<std::string, std::string>& headers) {
    std::string cmd = "-s -L";
    for (const auto& [key, val] : headers) {
        cmd += " -H \"" + key + ": " + val + "\"";
    }
    cmd += " \"" + url + "\"";
    
    std::string output;
    RunCommand(cmd, output);
    return output;
}

std::string HTTPAgentModule::Post(const std::string& url, const std::string& body, const std::map<std::string, std::string>& headers) {
    std::string cmd = "-s -L -X POST";
    for (const auto& [key, val] : headers) {
        cmd += " -H \"" + key + ": " + val + "\"";
    }
    cmd += " -d '" + body + "'";
    cmd += " \"" + url + "\"";
    
    std::string output;
    RunCommand(cmd, output);
    return output;
}

bool HTTPAgentModule::DownloadFile(const std::string& url, const std::string& destinationPath) {
    std::string cmd = "-s -L -o \"" + destinationPath + "\" \"" + url + "\"";
    std::string output;
    return RunCommand(cmd, output);
}

std::string HTTPAgentModule::DispatchSecureHttpsAsync(const SecureHttpsRequest& request,
                                                       std::function<void(SecureHttpsResponse)> callback) {
    static std::atomic<unsigned long long> nextRequestId{1};
    const auto requestId = "https-" + std::to_string(nextRequestId.fetch_add(1));

    // Do not retain the caller's map beyond this dispatch.  In particular the
    // Authorization header lives only in this worker copy until WinHTTP has
    // written it to the TLS connection.
    std::thread([request, callback = std::move(callback)]() mutable {
        SecureHttpsResponse response;
#if defined(_WIN32)
        if (request.url.rfind("https://", 0) != 0 || request.url.size() > 2048 ||
            (request.method != "GET" && request.method != "POST") || request.timeoutMs == 0 ||
            request.timeoutMs > 30000 || request.maxResponseBytes == 0 || request.maxResponseBytes > 1048576) {
            response.error = "Rejected insecure or invalid HTTPS request.";
        } else {
            URL_COMPONENTS parts{};
            parts.dwStructSize = sizeof(parts);
            parts.dwSchemeLength = static_cast<DWORD>(-1);
            parts.dwHostNameLength = static_cast<DWORD>(-1);
            parts.dwUrlPathLength = static_cast<DWORD>(-1);
            parts.dwExtraInfoLength = static_cast<DWORD>(-1);
            parts.dwUserNameLength = static_cast<DWORD>(-1);
            parts.dwPasswordLength = static_cast<DWORD>(-1);
            std::wstring wideUrl(request.url.begin(), request.url.end());
            if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS ||
                parts.dwHostNameLength == 0 || parts.dwUserNameLength != 0 || parts.dwPasswordLength != 0) {
                response.error = "Rejected malformed HTTPS URL.";
            } else {
                const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
                std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
                if (parts.dwExtraInfoLength > 0) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
                const std::wstring method(request.method.begin(), request.method.end());
                HINTERNET session = WinHttpOpen(L"CelestiaNova/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
                if (!session) {
                    response.error = "HTTPS transport initialization failed.";
                } else {
                    WinHttpSetTimeouts(session, static_cast<int>(request.timeoutMs), static_cast<int>(request.timeoutMs),
                                       static_cast<int>(request.timeoutMs), static_cast<int>(request.timeoutMs));
                    HINTERNET connection = WinHttpConnect(session, host.c_str(), parts.nPort, 0);
                    HINTERNET httpRequest = connection ? WinHttpOpenRequest(connection, method.c_str(), path.c_str(), nullptr,
                                                                              WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                                              WINHTTP_FLAG_SECURE) : nullptr;
                    if (!httpRequest) {
                        response.error = "HTTPS connection setup failed.";
                    } else {
                        std::wstring headers;
                        bool validHeaders = true;
                        for (const auto& [key, value] : request.headers) {
                            if (key.empty() || key.find_first_of("\r\n:") != std::string::npos ||
                                value.find_first_of("\r\n") != std::string::npos) { validHeaders = false; break; }
                            headers += std::wstring(key.begin(), key.end()) + L": " + std::wstring(value.begin(), value.end()) + L"\r\n";
                        }
                        if (!validHeaders || request.body.size() > 1048576) {
                            response.error = "Rejected unsafe HTTPS request headers or body.";
                        } else if (WinHttpSendRequest(httpRequest, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                                                      headers.empty() ? 0 : static_cast<DWORD>(headers.size()),
                                                      request.body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(request.body.data()),
                                                      static_cast<DWORD>(request.body.size()), static_cast<DWORD>(request.body.size()), 0) &&
                                   WinHttpReceiveResponse(httpRequest, nullptr)) {
                            DWORD status = 0, statusSize = sizeof(status);
                            WinHttpQueryHeaders(httpRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                                WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
                            response.statusCode = status;
                            for (;;) {
                                DWORD available = 0;
                                if (!WinHttpQueryDataAvailable(httpRequest, &available)) { response.error = "HTTPS response read failed."; break; }
                                if (available == 0) { response.transportSucceeded = true; break; }
                                if (response.body.size() + available > request.maxResponseBytes) { response.error = "HTTPS response exceeded configured limit."; break; }
                                std::string chunk(available, '\0'); DWORD read = 0;
                                if (!WinHttpReadData(httpRequest, chunk.data(), available, &read)) { response.error = "HTTPS response read failed."; break; }
                                response.body.append(chunk.data(), read);
                            }
                        } else {
                            response.error = "HTTPS request failed.";
                        }
                    }
                    if (httpRequest) WinHttpCloseHandle(httpRequest);
                    if (connection) WinHttpCloseHandle(connection);
                    WinHttpCloseHandle(session);
                }
            }
        }
#else
        (void)request;
        response.error = "Secure HTTPS transport is unavailable on this platform.";
#endif
        if (callback) callback(std::move(response));
    }).detach();
    return requestId;
}

} // namespace Core

extern "C" bool HTTPAgent_Resolve(const void* requestPtr, void* resultPtr) {
    auto* instance = Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("httpagent");
    if (!instance) return false;
    auto* agent = dynamic_cast<Core::HTTPAgentModule*>(instance);
    if (!agent) return false;
    return Core::RequirementResolver::DispatchResolveRequest(requestPtr, resultPtr, [&](const auto& req) {
        return agent->Resolve(req);
    });
}
