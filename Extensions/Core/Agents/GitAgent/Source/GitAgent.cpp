#include "GitAgent.h"
#include "Core/NovaLog.h"
#include "Core/ExtensionRegistry.h"
#include "TerminalAgent.h"
#include "ExtensionSpecific/IPackageManagerAgent.h"
#include <chrono>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <thread>

namespace Core {

namespace {

bool IsSafeGitHttpsUrl(const std::string& value) {
    if (value.rfind("https://", 0) != 0 || value.size() <= 8) {
        return false;
    }
    for (const unsigned char character : value) {
        if (!(std::isalnum(character) || character == ':' || character == '/' ||
              character == '.' || character == '-' || character == '_')) {
            return false;
        }
    }
    return true;
}

bool IsSafeGitRef(const std::string& value) {
    if (value.empty() || value.find("..") != std::string::npos) {
        return false;
    }
    for (const unsigned char character : value) {
        if (!(std::isalnum(character) || character == '/' || character == '.' ||
              character == '-' || character == '_')) {
            return false;
        }
    }
    return true;
}

bool IsSafeAbsoluteDestination(const std::string& value) {
    if (value.empty() || value.find("..") != std::string::npos) {
        return false;
    }

    // ContentForge hands us an absolute runtime cache location.  Unix uses
    // /var/..., while Windows uses a drive-qualified path such as E:\\....
    // `std::filesystem` validates both forms for the active platform.
    const std::filesystem::path destination(value);
    if (!destination.is_absolute()) return false;
    for (const auto& component : destination) {
        if (component == "..") return false;
    }
    for (const unsigned char character : value) {
        if (!(std::isalnum(character) || character == '/' || character == '\\' ||
              character == ':' || character == '.' || character == '-' || character == '_')) {
            return false;
        }
    }
    return true;
}

} // namespace

GitAgentModule::GitAgentModule() = default;
GitAgentModule::~GitAgentModule() = default;

void GitAgentModule::StartupModule() {
    NOVA_LOG("GitAgent starting up...", LogType::Log);
}

void GitAgentModule::ShutdownModule() {
    NOVA_LOG("GitAgent shutting down...", LogType::Log);
}

void GitAgentModule::AddLog(const std::string& message) {
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

CanvasMenuActionResult GitAgentModule::OnMenuAction(const CanvasMenuActionRequest& request) {
    CanvasMenuActionResult result;
    result.Success = true;

    if (request.ActionId == "git.action.check_version") {
        std::thread([this]() {
            std::string output;
            RunCommand("--version", output);
        }).detach();
        result.NavigateToMenuId = "gitagent::git_progress";
    } else if (request.ActionId == "git.action.clone") {
        std::string url;
        std::string destination;
        std::string branch;

        auto urlIt = request.ContextValues.find("repo_url");
        if (urlIt != request.ContextValues.end()) url = urlIt->second;
        auto destIt = request.ContextValues.find("repo_destination");
        if (destIt != request.ContextValues.end()) destination = destIt->second;
        auto branchIt = request.ContextValues.find("repo_branch");
        if (branchIt != request.ContextValues.end()) branch = branchIt->second;

        if (!url.empty() && !destination.empty()) {
            std::thread([this, url, destination, branch]() {
                Clone(url, destination, branch);
            }).detach();
            result.NavigateToMenuId = "gitagent::git_progress";
        } else {
            result.Success = false;
            result.ErrorMessage = "Repository URL and destination are required.";
        }
    } else if (request.ActionId == "git.action.pull") {
        std::string repoPath = ".";
        auto pathIt = request.ContextValues.find("repo_path");
        if (pathIt != request.ContextValues.end() && !pathIt->second.empty()) {
            repoPath = pathIt->second;
        }

        std::thread([this, repoPath]() {
            Pull(repoPath);
        }).detach();
        result.NavigateToMenuId = "gitagent::git_progress";
    }

    return result;
}

Core::RequirementResolver::CoreRequirementResolveResult GitAgentModule::Resolve(const Core::RequirementResolver::CoreRequirementResolveRequest& request) {
    Core::RequirementResolver::CoreRequirementResolveResult result;
    result.Success = true;

    if (request.RequirementKey == "git.logs") {
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

bool GitAgentModule::IsInstalled() const {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return false;

    CoreTerminal::TerminalCommandRequest req;
    req.command = "git --version";
    auto result = terminalAgent->ExecuteCommandSync(req);
    return result.exitCode == 0;
}

bool GitAgentModule::Install(std::function<void(const std::string&)> onProgress) {
    if (IsInstalled()) {
        if (onProgress) onProgress("Git is already installed.");
        return true;
    }

    auto pmAgent = dynamic_cast<IPackageManagerAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("packagemanageragent"));
    if (!pmAgent) return false;

    AddLog("Requesting Git installation via PackageManagerAgent...");
    return pmAgent->InstallPackage("git");
}

bool GitAgentModule::Uninstall() {
    auto pmAgent = dynamic_cast<IPackageManagerAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("packagemanageragent"));
    if (!pmAgent) return false;

    AddLog("Requesting Git uninstallation via PackageManagerAgent...");
    return pmAgent->UninstallPackage("git");
}

bool GitAgentModule::RunCommand(const std::string& command, std::string& outOutput) {
    auto terminalAgent = dynamic_cast<CoreTerminal::ITerminalAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminalAgent) return false;

    AddLog("Running git " + command);

    CoreTerminal::TerminalCommandRequest req;
    req.command = "git " + command;
    auto result = terminalAgent->ExecuteCommandSync(req);
    
    outOutput = result.stdOut;
    AddLog(result.stdOut);
    if (result.exitCode != 0) {
        AddLog("Error: " + result.stdErr);
    }
    return result.exitCode == 0;
}

bool GitAgentModule::Configure(const std::string& configKey, const std::string& configValue) {
    std::string output;
    return RunCommand("config --global " + configKey + " \"" + configValue + "\"", output);
}

bool GitAgentModule::Clone(const std::string& url, const std::string& destination, const std::string& branch) {
    // Clone is used by ContentForge in service mode.  It only accepts a
    // shell-safe HTTPS URL, a declared ref, and an absolute cache location.
    // This keeps the legacy TerminalAgent bridge from becoming an injection
    // primitive for declarative content manifests.
    if (!IsSafeGitHttpsUrl(url) || !IsSafeAbsoluteDestination(destination) ||
        (!branch.empty() && !IsSafeGitRef(branch))) {
        AddLog("Rejected an unsafe Git clone declaration.");
        return false;
    }
    std::string cmd = "clone --depth 1";
    if (!branch.empty()) {
        cmd += " --branch " + branch;
    }
    cmd += " -- " + url + " " + destination;
    std::string output;
    return RunCommand(cmd, output);
}

bool GitAgentModule::Pull(const std::string& repoPath) {
    std::string output;
    return RunCommand("-C " + repoPath + " pull", output);
}

bool GitAgentModule::Push(const std::string& repoPath, const std::string& message) {
    std::string output;
    if (!RunCommand("-C " + repoPath + " add .", output)) return false;
    if (!RunCommand("-C " + repoPath + " commit -m \"" + message + "\"", output)) return false;
    return RunCommand("-C " + repoPath + " push", output);
}

std::string GitAgentModule::GetCurrentBranch(const std::string& repoPath) {
    std::string output;
    if (RunCommand("-C " + repoPath + " rev-parse --abbrev-ref HEAD", output)) {
        output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
        output.erase(std::remove(output.begin(), output.end(), '\r'), output.end());
        return output;
    }
    return "";
}

std::string GitAgentModule::GetCurrentRevision(const std::string& repoPath) {
    std::string output;
    if (RunCommand("-C " + repoPath + " rev-parse HEAD", output)) {
        output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
        output.erase(std::remove(output.begin(), output.end(), '\r'), output.end());
        return output;
    }
    return "";
}

bool GitAgentModule::IsRepo(const std::string& path) {
    std::string output;
    return RunCommand("-C " + path + " rev-parse --is-inside-work-tree", output);
}

bool GitAgentModule::ExecuteCommandInternal(const std::string& command, std::string& outOutput) {
    // Legacy method, redirected to RunCommand which uses TerminalAgent
    return RunCommand(command.substr(4), outOutput); // Remove "git " prefix if present
}

} // namespace Core

extern "C" bool GitAgent_Resolve(const void* requestPtr, void* resultPtr) {
    auto* instance = Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("git-agent");
    if (!instance) return false;
    auto* agent = dynamic_cast<Core::GitAgentModule*>(instance);
    if (!agent) return false;
    return Core::RequirementResolver::DispatchResolveRequest(requestPtr, resultPtr, [&](const auto& req) {
        return agent->Resolve(req);
    });
}
