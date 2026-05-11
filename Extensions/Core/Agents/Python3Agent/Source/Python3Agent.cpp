#include "Python3Agent.h"
#include "NovaLog.h"
#include "Core/ExtensionRegistry.h"
#include "TerminalAgent.h"
#include "ExtensionSpecific/IPackageManagerAgent.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <array>
#include <memory>
#include <algorithm>
#include <sstream>
#include <thread>

namespace Core {

Python3AgentModule::Python3AgentModule() = default;
Python3AgentModule::~Python3AgentModule() = default;

void Python3AgentModule::StartupModule() {
    NOVA_LOG("Python3Agent starting up...", LogType::Log);
}

void Python3AgentModule::ShutdownModule() {
    NOVA_LOG("Python3Agent shutting down...", LogType::Log);
}

void Python3AgentModule::AddLog(const std::string& message) {
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

CanvasMenuActionResult Python3AgentModule::OnMenuAction(const CanvasMenuActionRequest& request) {
    CanvasMenuActionResult result;
    result.Success = true;

    if (request.ActionId == "python.action.check_version") {
        std::thread([this]() {
            GetPythonVersion();
        }).detach();
        result.NavigateToMenuId = "python3agent::python_progress";
    } else if (request.ActionId == "python.action.run_script") {
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
                RunScript(scriptPath, args, "");
            }).detach();
            result.NavigateToMenuId = "python3agent::python_progress";
        } else {
            result.Success = false;
            result.ErrorMessage = "Script path is required.";
        }
    }

    return result;
}

Core::RequirementResolver::CoreRequirementResolveResult Python3AgentModule::Resolve(const Core::RequirementResolver::CoreRequirementResolveRequest& request) {
    Core::RequirementResolver::CoreRequirementResolveResult result;
    result.Success = true;

    if (request.RequirementKey == "python.logs") {
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

bool Python3AgentModule::IsInstalled() const {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return false;

    CoreTerminal::TerminalCommandRequest req;
    req.command = "python3 --version";
    auto result = terminalAgent->ExecuteCommandSync(req);
    return result.exitCode == 0;
}

bool Python3AgentModule::Install(std::function<void(const std::string&)> onProgress) {
    if (IsInstalled()) {
        if (onProgress) onProgress("Python 3 is already installed.");
        return true;
    }

    auto pmAgent = dynamic_cast<IPackageManagerAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("packagemanageragent"));
    if (!pmAgent) return false;

    AddLog("Requesting Python3 installation via PackageManagerAgent...");
    return pmAgent->InstallPackage("python3");
}

bool Python3AgentModule::Uninstall() {
    auto pmAgent = dynamic_cast<IPackageManagerAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("packagemanageragent"));
    if (!pmAgent) return false;

    AddLog("Requesting Python3 uninstallation via PackageManagerAgent...");
    return pmAgent->UninstallPackage("python3");
}

bool Python3AgentModule::RunCommand(const std::string& command, std::string& outOutput) {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return false;

    AddLog("Running python3 " + command);

    CoreTerminal::TerminalCommandRequest req;
    req.command = "python3 " + command;
    auto result = terminalAgent->ExecuteCommandSync(req);
    
    outOutput = result.stdOut;
    AddLog(result.stdOut);
    return result.exitCode == 0;
}

bool Python3AgentModule::Configure(const std::string& configKey, const std::string& configValue) {
    return true;
}

bool Python3AgentModule::CreateVirtualEnv(const std::string& path) {
    std::string output;
    return RunCommand("-m venv " + path, output);
}

bool Python3AgentModule::PipInstall(const std::string& package, const std::string& venvPath) {
    std::string pipCmd = venvPath.empty() ? "pip3 install " : venvPath + "/bin/pip install ";
    if (package.find(".txt") != std::string::npos) {
        pipCmd += "-r " + package;
    } else {
        pipCmd += package;
    }
    
    std::string output;
    return ExecuteCommandInternal(pipCmd, output);
}

bool Python3AgentModule::RunScript(const std::string& scriptPath, const std::vector<std::string>& args, const std::string& venvPath) {
    std::string pyCmd = venvPath.empty() ? scriptPath : venvPath + "/bin/python " + scriptPath;
    for (const auto& arg : args) {
        pyCmd += " " + arg;
    }
    
    std::string output;
    return RunCommand(pyCmd, output);
}

std::string Python3AgentModule::GetPythonVersion() {
    std::string output;
    if (RunCommand("--version", output)) {
        return output;
    }
    return "Unknown";
}

bool Python3AgentModule::ExecuteCommandInternal(const std::string& command, std::string& outOutput) {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return false;

    CoreTerminal::TerminalCommandRequest req;
    req.command = command;
    auto result = terminalAgent->ExecuteCommandSync(req);
    outOutput = result.stdOut;
    AddLog(outOutput);
    return result.exitCode == 0;
}

} // namespace Core

extern "C" bool Python3Agent_Resolve(const void* requestPtr, void* resultPtr) {
    auto* instance = Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("python3-agent");
    if (!instance) return false;
    auto* agent = dynamic_cast<Core::Python3AgentModule*>(instance);
    if (!agent) return false;
    return Core::RequirementResolver::DispatchResolveRequest(requestPtr, resultPtr, [&](const auto& req) {
        return agent->Resolve(req);
    });
}
