#include "ComposerAgent.h"
#include "Core/ExtensionRegistry.h"
#include "Core/NovaLog.h"
#include "TerminalAgent.h"
#include "ExtensionSpecific/IPackageManagerAgent.h"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace Core {

ComposerAgentModule::ComposerAgentModule() {}
ComposerAgentModule::~ComposerAgentModule() {}

void ComposerAgentModule::StartupModule() {
    NOVA_LOG("[ComposerAgent] StartupModule called. PHP Composer integration ready.", LogType::Log);
}

void ComposerAgentModule::ShutdownModule() {
    NOVA_LOG("[ComposerAgent] ShutdownModule called.", LogType::Log);
}

bool ComposerAgentModule::IsInstalled() const {
    return IsComposerInstalled();
}

bool ComposerAgentModule::Install(std::function<void(const std::string&)> onProgress) {
    return InstallComposer();
}

bool ComposerAgentModule::Uninstall() {
    return false;
}

bool ComposerAgentModule::RunCommand(const std::string& command, std::string& outOutput) {
    return false;
}

bool ComposerAgentModule::Configure(const std::string& configKey, const std::string& configValue) {
    return false;
}

void ComposerAgentModule::AddLog(const std::string& message) {
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

CanvasMenuActionResult ComposerAgentModule::OnMenuAction(const CanvasMenuActionRequest& request) {
    CanvasMenuActionResult result;
    result.Success = true;

    if (request.ActionId == "composer.action.install_composer") {
        InstallComposer();
        result.NavigateToMenuId = "composer_progress";
    } else if (request.ActionId == "composer.action.validate_config") {
        ValidateConfig(".");
        result.NavigateToMenuId = "composer_progress";
    }

    return result;
}

Core::RequirementResolver::CoreRequirementResolveResult ComposerAgentModule::Resolve(const Core::RequirementResolver::CoreRequirementResolveRequest& request) {
    Core::RequirementResolver::CoreRequirementResolveResult result;
    result.Success = true;

    if (request.RequirementKey == "composer.logs") {
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

bool ComposerAgentModule::IsComposerInstalled() const {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return false;

    CoreTerminal::TerminalCommandRequest req;
    req.command = "composer --version";
    auto result = terminalAgent->ExecuteCommandSync(req);
    return result.exitCode == 0;
}

bool ComposerAgentModule::InstallComposer() const {
    auto pmAgent = dynamic_cast<IPackageManagerAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("packagemanageragent"));
    if (!pmAgent) return false;

    // Use a const_cast or similar if we need to call AddLog from a const method
    // But since this is a module, we can just call it.
    const_cast<ComposerAgentModule*>(this)->AddLog("Starting Composer installation...");
    bool success = pmAgent->InstallPackage("composer");
    const_cast<ComposerAgentModule*>(this)->AddLog(success ? "Composer installed successfully." : "Composer installation failed.");
    return success;
}

std::string ComposerAgentModule::GetComposerCommand(const std::string& workingDir) const {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return "composer"; // fallback

#if defined(_WIN32)
    std::string sep = "\\";
    CoreTerminal::TerminalCommandRequest req;
    req.command = "if exist \"" + workingDir + sep + "composer.phar\" echo local";
#else
    std::string sep = "/";
    CoreTerminal::TerminalCommandRequest req;
    req.command = "if [ -f \"" + workingDir + sep + "composer.phar\" ]; then echo local; fi";
#endif

    auto res = terminalAgent->ExecuteCommandSync(req);
    if (res.exitCode == 0 && res.stdOut.find("local") != std::string::npos) {
        return "php composer.phar";
    }

    return "composer";
}

bool ComposerAgentModule::InstallDependencies(const std::string& workingDir, bool noDev) const {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return false;

    std::string compCmd = GetComposerCommand(workingDir);
    const_cast<ComposerAgentModule*>(this)->AddLog("Running '" + compCmd + " install' in " + workingDir);

    CoreTerminal::TerminalCommandRequest req;
    req.command = compCmd + " install";
    if (noDev) req.command += " --no-dev";
    req.workingDirectory = workingDir;

    std::string cmdId = terminalAgent->ExecuteCommandAsync(req, [this](CoreTerminal::TerminalCommandResult res) {
        const_cast<ComposerAgentModule*>(this)->AddLog("Composer Install finished with code " + std::to_string(res.exitCode));
    });

    terminalAgent->StreamCommandOutput(cmdId, [this](const std::string& output) {
        const_cast<ComposerAgentModule*>(this)->AddLog(output);
    });

    return true;
}

bool ComposerAgentModule::UpdateDependencies(const std::string& workingDir) const {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return false;

    std::string compCmd = GetComposerCommand(workingDir);
    const_cast<ComposerAgentModule*>(this)->AddLog("Running '" + compCmd + " update' in " + workingDir);

    CoreTerminal::TerminalCommandRequest req;
    req.command = compCmd + " update";
    req.workingDirectory = workingDir;

    std::string cmdId = terminalAgent->ExecuteCommandAsync(req, [this](CoreTerminal::TerminalCommandResult res) {
        const_cast<ComposerAgentModule*>(this)->AddLog("Composer Update finished with code " + std::to_string(res.exitCode));
    });

    terminalAgent->StreamCommandOutput(cmdId, [this](const std::string& output) {
        const_cast<ComposerAgentModule*>(this)->AddLog(output);
    });

    return true;
}

bool ComposerAgentModule::ValidateConfig(const std::string& workingDir) const {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return false;

    std::string compCmd = GetComposerCommand(workingDir);
    const_cast<ComposerAgentModule*>(this)->AddLog("Validating composer.json in " + workingDir);

    CoreTerminal::TerminalCommandRequest req;
    req.command = compCmd + " validate";
    req.workingDirectory = workingDir;

    std::string cmdId = terminalAgent->ExecuteCommandAsync(req, [this](CoreTerminal::TerminalCommandResult res) {
        const_cast<ComposerAgentModule*>(this)->AddLog("Composer Validate finished with code " + std::to_string(res.exitCode));
    });

    terminalAgent->StreamCommandOutput(cmdId, [this](const std::string& output) {
        const_cast<ComposerAgentModule*>(this)->AddLog(output);
    });

    return true;
}

} // namespace Core

extern "C" bool ComposerAgent_Resolve(const void* requestPtr, void* resultPtr) {
    auto* instance = Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("composeragent");
    if (!instance) return false;
    auto* agent = dynamic_cast<Core::ComposerAgentModule*>(instance);
    if (!agent) return false;
    return Core::RequirementResolver::DispatchResolveRequest(requestPtr, resultPtr, [&](const auto& req) {
        return agent->Resolve(req);
    });
}
