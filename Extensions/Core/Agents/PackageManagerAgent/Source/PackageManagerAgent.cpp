#include "PackageManagerAgent.h"
#include "NovaLog.h"
#include "NovaPlatforms.h"
#include "Core/ExtensionRegistry.h"
#include "ExtensionSpecific/IPrivilegeEscalationAgent.h"
#include <iostream>
#include <array>
#include <memory>
#include <cstdio>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace {
std::string GetElevatedPrefix() {
    auto& registry = Core::ExtensionRegistry::Instance();
    auto* pe = dynamic_cast<Core::IPrivilegeEscalationAgent*>(registry.GetLoadedExtensionInstance("privilegeescalationagent"));
    if (pe) {
        return pe->GetElevatedCommandPrefix();
    }
    return "sudo "; // Fallback
}
}

namespace Core {

PackageManagerAgentModule::PackageManagerAgentModule() = default;
PackageManagerAgentModule::~PackageManagerAgentModule() = default;

void PackageManagerAgentModule::StartupModule() {
    NOVA_LOG("PackageManagerAgent starting up...", LogType::Log);
}

void PackageManagerAgentModule::ShutdownModule() {
    NOVA_LOG("PackageManagerAgent shutting down...", LogType::Log);
}

bool PackageManagerAgentModule::IsInstalled() const {
    std::string checkCmd = GetPlatformCheckCommand();
#if defined(_WIN32)
    return system((checkCmd + " > nul 2>&1").c_str()) == 0;
#else
    return system((checkCmd + " > /dev/null 2>&1").c_str()) == 0;
#endif
}

bool PackageManagerAgentModule::Install(std::function<void(const std::string&)> onProgress) {
    if (IsInstalled()) {
        if (onProgress) onProgress("Package manager is already installed.");
        return true;
    }

    if (onProgress) onProgress("Installing package manager...");
    std::string cmd = GetPlatformInstallCommand();
    std::string output;
    return ExecuteCommandInternal(cmd, output);
}

bool PackageManagerAgentModule::Uninstall() {
    std::string cmd = GetPlatformUninstallCommand();
    if (cmd.empty()) return false;
    std::string output;
    return ExecuteCommandInternal(cmd, output);
}

bool PackageManagerAgentModule::RunCommand(const std::string& command, std::string& outOutput) {
    return ExecuteCommandInternal(command, outOutput);
}

bool PackageManagerAgentModule::Configure(const std::string& configKey, const std::string& configValue) {
    // Placeholder for configuration logic
    return true;
}

void PackageManagerAgentModule::AddLog(const std::string& message) {
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

void PackageManagerAgentModule::SetProgress(float progress, const std::string& step) {
    std::lock_guard<std::mutex> lock(ProgressMutex_);
    CurrentProgress_ = progress;
    CurrentStep_ = step;
}

CanvasMenuActionResult PackageManagerAgentModule::OnMenuAction(const CanvasMenuActionRequest& request) {
    return CanvasMenuActionResult();
}

Core::RequirementResolver::CoreRequirementResolveResult PackageManagerAgentModule::Resolve(const Core::RequirementResolver::CoreRequirementResolveRequest& request) {
    Core::RequirementResolver::CoreRequirementResolveResult result;
    result.Success = true;

    std::lock_guard<std::mutex> lock(ProgressMutex_);
    if (request.RequirementKey == "package_manager.progress") {
        Core::RequirementResolver::CoreRequirementResolvedOption option;
        option.Value = std::to_string(CurrentProgress_);
        result.Options.push_back(option);
    } else if (request.RequirementKey == "package_manager.step") {
        Core::RequirementResolver::CoreRequirementResolvedOption option;
        option.Value = CurrentStep_;
        result.Options.push_back(option);
    } else if (request.RequirementKey == "package_manager.logs") {
        std::string allLogs;
        for (const auto& log : Logs_) {
            allLogs += "[" + log.Timestamp + "] " + log.Message + "\n";
        }
        Core::RequirementResolver::CoreRequirementResolvedOption option;
        option.Value = allLogs;
        result.Options.push_back(option);
    } else {
        result.Success = false;
        result.ErrorMessage = "Unknown requirement key: " + request.RequirementKey;
    }

    return result;
}

bool PackageManagerAgentModule::InstallPackage(const std::string& packageName) {
    SetProgress(0.1f, "INSTALLING_" + packageName);
    AddLog("Starting installation of package: " + packageName);
    
    std::string manager = GetUnderlyingManagerName();
    std::string cmd;
    if (manager == "brew") cmd = "brew install " + packageName;
    else if (manager == "choco") cmd = "choco install " + packageName + " -y";
    else if (manager == "apt") cmd = GetElevatedPrefix() + "apt-get install -y " + packageName;
    else {
        AddLog("Error: Unsupported package manager");
        return false;
    }
    
    std::string output;
    bool success = ExecuteCommandInternal(cmd, output);
    
    AddLog(output);
    if (success) {
        SetProgress(1.0f, "COMPLETED");
        AddLog("Successfully installed " + packageName);
    } else {
        SetProgress(1.0f, "FAILED");
        AddLog("Failed to install " + packageName);
    }
    
    return success;
}

bool PackageManagerAgentModule::UninstallPackage(const std::string& packageName) {
    SetProgress(0.1f, "UNINSTALLING_" + packageName);
    AddLog("Starting uninstallation of package: " + packageName);

    std::string manager = GetUnderlyingManagerName();
    std::string cmd;
    if (manager == "brew") cmd = "brew uninstall " + packageName;
    else if (manager == "choco") cmd = "choco uninstall " + packageName + " -y";
    else if (manager == "apt") cmd = GetElevatedPrefix() + "apt-get remove -y " + packageName;
    else {
        AddLog("Error: Unsupported package manager");
        return false;
    }

    std::string output;
    bool success = ExecuteCommandInternal(cmd, output);
    
    AddLog(output);
    if (success) {
        SetProgress(1.0f, "REMOVED");
        AddLog("Successfully uninstalled " + packageName);
    } else {
        SetProgress(1.0f, "FAILED");
        AddLog("Failed to uninstall " + packageName);
    }

    return success;
}

bool PackageManagerAgentModule::IsPackageInstalled(const std::string& packageName) {
    std::string manager = GetUnderlyingManagerName();
    std::string cmd;
    if (manager == "brew") cmd = "brew list " + packageName;
    else if (manager == "choco") cmd = "choco list --local-only " + packageName;
    else if (manager == "apt") cmd = "dpkg -l " + packageName;
    else return false;

    std::string output;
    return ExecuteCommandInternal(cmd, output);
}

bool PackageManagerAgentModule::UpdateAll() {
    SetProgress(0.1f, "UPDATING_ALL");
    AddLog("Starting global package update...");

    std::string manager = GetUnderlyingManagerName();
    std::string cmd;
    if (manager == "brew") cmd = "brew update && brew upgrade";
    else if (manager == "choco") cmd = "choco upgrade all -y";
    else if (manager == "apt") cmd = GetElevatedPrefix() + "apt-get update && " + GetElevatedPrefix() + "apt-get upgrade -y";
    else return false;

    std::string output;
    bool success = ExecuteCommandInternal(cmd, output);
    
    AddLog(output);
    SetProgress(1.0f, success ? "COMPLETED" : "FAILED");
    
    return success;
}

std::string PackageManagerAgentModule::GetUnderlyingManagerName() const {
#if defined(__APPLE__)
    return "brew";
#elif defined(_WIN32)
    return "choco";
#else
    return "apt";
#endif
}

bool PackageManagerAgentModule::ExecuteCommandInternal(const std::string& command, std::string& outOutput) {
    std::array<char, 128> buffer;
    outOutput.clear();
    
#if defined(_WIN32)
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif

    if (!pipe) {
        return false;
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        outOutput += buffer.data();
    }

#if defined(_WIN32)
    int result = _pclose(pipe);
#else
    int result = pclose(pipe);
#endif

    return result == 0;
}

std::string PackageManagerAgentModule::GetPlatformInstallCommand() const {
#if defined(__APPLE__)
    return "/bin/bash -c \"$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\"";
#elif defined(_WIN32)
    return "powershell -NoProfile -ExecutionPolicy Bypass -Command \"[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))\"";
#else
    return GetElevatedPrefix() + "apt-get update"; 
#endif
}

std::string PackageManagerAgentModule::GetPlatformUninstallCommand() const {
#if defined(__APPLE__)
    return "/bin/bash -c \"$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/uninstall.sh)\"";
#else
    return ""; 
#endif
}

std::string PackageManagerAgentModule::GetPlatformCheckCommand() const {
#if defined(_WIN32)
    return "where choco";
#else
    return "which " + GetUnderlyingManagerName();
#endif
}

} // namespace Core

extern "C" PACKAGEMANAGERAGENT_API bool PackageManagerAgent_Resolve(const void* requestPtr, void* resultPtr) {
    auto& registry = Core::ExtensionRegistry::Instance();
    auto* instance = registry.GetLoadedExtensionInstance("packagemanageragent");
    if (!instance) return false;
    
    auto* agent = dynamic_cast<Core::PackageManagerAgentModule*>(instance);
    if (!agent) return false;
    
    return Core::RequirementResolver::DispatchResolveRequest(requestPtr, resultPtr, [&](const auto& req) {
        return agent->Resolve(req);
    });
}
