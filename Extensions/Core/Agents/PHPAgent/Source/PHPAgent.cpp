#include "PHPAgent.h"
#include "Core/ExtensionRegistry.h"
#include "Core/NovaLog.h"
#include "TerminalAgent.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

namespace Core {

PHPAgentModule::PHPAgentModule() {}
PHPAgentModule::~PHPAgentModule() {}

void PHPAgentModule::StartupModule() {
    NOVA_LOG("[PHPAgent] StartupModule called. PHP environment ready.", LogType::Log);
}

void PHPAgentModule::ShutdownModule() {
    NOVA_LOG("[PHPAgent] ShutdownModule called.", LogType::Log);
}

void PHPAgentModule::AddLog(const std::string& message) {
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

CanvasMenuActionResult PHPAgentModule::OnMenuAction(const CanvasMenuActionRequest& request) {
    CanvasMenuActionResult result;
    result.Success = true;

    if (request.ActionId == "php.action.check_version") {
        std::thread([this]() {
            GetPHPVersion();
        }).detach();
        result.NavigateToMenuId = "phpagent::php_progress";
    } else if (request.ActionId == "php.action.run_script") {
        std::string scriptPath;
        auto pathIt = request.ContextValues.find("script_path");
        if (pathIt != request.ContextValues.end()) {
            scriptPath = pathIt->second;
        }

        std::vector<std::string> args;
        auto argsIt = request.ContextValues.find("script_args");
        if (argsIt != request.ContextValues.end()) {
            std::istringstream stream(argsIt->second);
            std::string token;
            while (stream >> token) {
                args.push_back(token);
            }
        }

        if (!scriptPath.empty()) {
            std::thread([this, scriptPath, args]() {
                RunScript(scriptPath, args);
            }).detach();
            result.NavigateToMenuId = "phpagent::php_progress";
        } else {
            result.Success = false;
            result.ErrorMessage = "Script path is required.";
        }
    }

    return result;
}

Core::RequirementResolver::CoreRequirementResolveResult PHPAgentModule::Resolve(const Core::RequirementResolver::CoreRequirementResolveRequest& request) {
    Core::RequirementResolver::CoreRequirementResolveResult result;
    result.Success = true;

    if (request.RequirementKey == "php.logs") {
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

std::string PHPAgentModule::GetPHPVersion() const {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return "Unknown";

    const_cast<PHPAgentModule*>(this)->AddLog("Checking PHP version...");

    CoreTerminal::TerminalCommandRequest req;
    req.command = "php -v";
    auto result = terminalAgent->ExecuteCommandSync(req);
    
    const_cast<PHPAgentModule*>(this)->AddLog(result.stdOut);
    return result.stdOut;
}

bool PHPAgentModule::IsExtensionLoaded(const std::string& extName) const {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return false;

    CoreTerminal::TerminalCommandRequest req;
    req.command = "php -m";
    auto result = terminalAgent->ExecuteCommandSync(req);
    
    return result.stdOut.find(extName) != std::string::npos;
}

bool PHPAgentModule::RunScript(const std::string& scriptPath, const std::vector<std::string>& args) const {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return false;

    const_cast<PHPAgentModule*>(this)->AddLog("Running PHP script: " + scriptPath);

    CoreTerminal::TerminalCommandRequest req;
    req.command = "php " + scriptPath;
    for (const auto& arg : args) {
        req.command += " " + arg;
    }

    std::string cmdId = terminalAgent->ExecuteCommandAsync(req, [this](CoreTerminal::TerminalCommandResult res) {
        const_cast<PHPAgentModule*>(this)->AddLog("PHP script finished with code " + std::to_string(res.exitCode));
    });

    terminalAgent->StreamCommandOutput(cmdId, [this](const std::string& output) {
        const_cast<PHPAgentModule*>(this)->AddLog(output);
    });

    return true;
}

std::string PHPAgentModule::GetIniPath() const {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return "";

    CoreTerminal::TerminalCommandRequest req;
    req.command = "php --ini";
    auto result = terminalAgent->ExecuteCommandSync(req);
    
    // Simple parsing for Loaded Configuration File
    size_t pos = result.stdOut.find("Loaded Configuration File:");
    if (pos != std::string::npos) {
        size_t start = result.stdOut.find(":", pos) + 1;
        size_t end = result.stdOut.find("\n", start);
        std::string path = result.stdOut.substr(start, end - start);
        // trim path
        path.erase(0, path.find_first_not_of(" \t\r\n"));
        path.erase(path.find_last_not_of(" \t\r\n") + 1);
        return path;
    }
    return "";
}

bool PHPAgentModule::IsInstalled() const {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return false;

    CoreTerminal::TerminalCommandRequest req;
    req.command = "php -v";
    auto result = terminalAgent->ExecuteCommandSync(req);

    return (result.exitCode == 0) || (result.stdOut.find("PHP") != std::string::npos);
}

bool PHPAgentModule::Install(std::function<void(const std::string&)> onProgress) {
    if (onProgress) onProgress("Install not implemented on this platform");
    NOVA_LOG("[PHPAgent] Install() called - not implemented", LogType::Warning);
    return false;
}

bool PHPAgentModule::Uninstall() {
    NOVA_LOG("[PHPAgent] Uninstall() called - not implemented", LogType::Warning);
    return false;
}

bool PHPAgentModule::RunCommand(const std::string& command, std::string& outOutput) {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return false;

    CoreTerminal::TerminalCommandRequest req;
    req.command = command;
    auto result = terminalAgent->ExecuteCommandSync(req);

    outOutput = result.stdOut;
    return result.exitCode == 0;
}

bool PHPAgentModule::Configure(const std::string& configKey, const std::string& configValue) {
    NovaLog::Warning("[PHPAgent] Configure() called for key: " + configKey);
    return false;
}

} // namespace Core

extern "C" bool PHPAgent_Resolve(const void* requestPtr, void* resultPtr) {
    auto* instance = Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("phpagent");
    if (!instance) return false;
    auto* agent = dynamic_cast<Core::PHPAgentModule*>(instance);
    if (!agent) return false;
    return Core::RequirementResolver::DispatchResolveRequest(requestPtr, resultPtr, [&](const auto& req) {
        return agent->Resolve(req);
    });
}
