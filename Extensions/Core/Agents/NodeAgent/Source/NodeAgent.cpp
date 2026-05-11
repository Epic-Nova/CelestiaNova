#include "NodeAgent.h"
#include "Core/NovaLog.h"
#include "Core/ExtensionRegistry.h"
#include "TerminalAgent.h"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace Core {

NodeAgentModule::NodeAgentModule() {}
NodeAgentModule::~NodeAgentModule() {}

void NodeAgentModule::StartupModule() {
    NOVA_LOG("[NodeAgent] StartupModule called. Javascript environment ready.", LogType::Log);
}

void NodeAgentModule::ShutdownModule() {
    NOVA_LOG("[NodeAgent] ShutdownModule called.", LogType::Log);
}

bool NodeAgentModule::IsInstalled() const {
    return IsNodeInstalled();
}

bool NodeAgentModule::Install(std::function<void(const std::string&)> onProgress) {
    return false;
}

bool NodeAgentModule::Uninstall() {
    return false;
}

bool NodeAgentModule::RunCommand(const std::string& command, std::string& outOutput) {
    return false;
}

bool NodeAgentModule::Configure(const std::string& configKey, const std::string& configValue) {
    return false;
}

void NodeAgentModule::AddLog(const std::string& message) {
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

CanvasMenuActionResult NodeAgentModule::OnMenuAction(const CanvasMenuActionRequest& request) {
    CanvasMenuActionResult result;
    result.Success = true;

    std::string workingDir = ".";
    auto dirIt = request.ContextValues.find("working_dir");
    if (dirIt != request.ContextValues.end() && !dirIt->second.empty()) {
        workingDir = dirIt->second;
    }

    if (request.ActionId == "node.action.run_install") {
        RunInstall(workingDir);
        result.NavigateToMenuId = "nodeagent::node_progress";
    } else if (request.ActionId == "node.action.run_dev") {
        std::string scriptName = "dev";
        auto scriptIt = request.ContextValues.find("dev_script");
        if (scriptIt != request.ContextValues.end() && !scriptIt->second.empty()) {
            scriptName = scriptIt->second;
        }
        RunScript(workingDir, scriptName);
        result.NavigateToMenuId = "nodeagent::node_progress";
    }

    return result;
}

Core::RequirementResolver::CoreRequirementResolveResult NodeAgentModule::Resolve(const Core::RequirementResolver::CoreRequirementResolveRequest& request) {
    Core::RequirementResolver::CoreRequirementResolveResult result;
    result.Success = true;

    if (request.RequirementKey == "node.logs") {
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

bool NodeAgentModule::IsNodeInstalled() const {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return false;

    CoreTerminal::TerminalCommandRequest req;
    req.command = "node --version";
    auto result = terminalAgent->ExecuteCommandSync(req);
    return result.exitCode == 0;
}

bool NodeAgentModule::RunInstall(const std::string& workingDir) const {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return false;

    const_cast<NodeAgentModule*>(this)->AddLog("Running package install in " + workingDir);

    CoreTerminal::TerminalCommandRequest req;
    req.command = DetectPackageManager(workingDir) + " install";
    req.workingDirectory = workingDir;

    std::string cmdId = terminalAgent->ExecuteCommandAsync(req, [this](CoreTerminal::TerminalCommandResult res) {
        const_cast<NodeAgentModule*>(this)->AddLog("Node Install finished with code " + std::to_string(res.exitCode));
    });

    terminalAgent->StreamCommandOutput(cmdId, [this](const std::string& output) {
        const_cast<NodeAgentModule*>(this)->AddLog(output);
    });

    return true;
}

bool NodeAgentModule::RunScript(const std::string& workingDir, const std::string& scriptName) const {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return false;

    const_cast<NodeAgentModule*>(this)->AddLog("Running script '" + scriptName + "' in " + workingDir);

    CoreTerminal::TerminalCommandRequest req;
    req.command = DetectPackageManager(workingDir) + " run " + scriptName;
    req.workingDirectory = workingDir;

    std::string cmdId = terminalAgent->ExecuteCommandAsync(req, [this, scriptName](CoreTerminal::TerminalCommandResult res) {
        const_cast<NodeAgentModule*>(this)->AddLog("Script '" + scriptName + "' finished with code " + std::to_string(res.exitCode));
    });

    terminalAgent->StreamCommandOutput(cmdId, [this](const std::string& output) {
        const_cast<NodeAgentModule*>(this)->AddLog(output);
    });

    return true;
}

std::string NodeAgentModule::DetectPackageManager(const std::string& workingDir) const {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return "npm"; // fallback

#if defined(_WIN32)
    std::string sep = "\\";
#else
    std::string sep = "/";
#endif

    // We can use a simple dir check or terminal commands
    // We'll just run 'dir' or 'ls' through terminal for checking
    CoreTerminal::TerminalCommandRequest req;
#if defined(_WIN32)
    req.command = "dir /b \"" + workingDir + "\"";
#else
    req.command = "ls -1 \"" + workingDir + "\"";
#endif
    
    auto res = terminalAgent->ExecuteCommandSync(req);
    if (res.exitCode == 0) {
        if (res.stdOut.find("pnpm-lock.yaml") != std::string::npos) return "pnpm";
        if (res.stdOut.find("yarn.lock") != std::string::npos) return "yarn";
    }

    return "npm";
}

} // namespace Core

extern "C" bool NodeAgent_Resolve(const void* requestPtr, void* resultPtr) {
    auto* instance = Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("nodeagent");
    if (!instance) return false;
    auto* agent = dynamic_cast<Core::NodeAgentModule*>(instance);
    if (!agent) return false;
    return Core::RequirementResolver::DispatchResolveRequest(requestPtr, resultPtr, [&](const auto& req) {
        return agent->Resolve(req);
    });
}
