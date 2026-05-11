#include "HTTPAgent.h"
#include "Core/ExtensionRegistry.h"
#include "Core/NovaLog.h"
#include "TerminalAgent.h"
#include "ExtensionSpecific/IPackageManagerAgent.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

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
