#include "PrivilegeEscalationAgent.h"
#include "Core/NovaLog.h"
#include <iostream>
#include <cstdio>
#include <array>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/types.h>
#endif

PrivilegeEscalationAgentModule::PrivilegeEscalationAgentModule() {}
PrivilegeEscalationAgentModule::~PrivilegeEscalationAgentModule() {}

void PrivilegeEscalationAgentModule::StartupModule() {
    NOVA_LOG("[PrivilegeEscalationAgent] StartupModule called.", LogType::Log);
}

void PrivilegeEscalationAgentModule::ShutdownModule() {
    Deauthenticate();
}

bool PrivilegeEscalationAgentModule::RunCommand(const std::string& command, std::string& outOutput) {
    std::string fullCommand;
#ifdef _WIN32
    if (PlatformCheckElevation()) {
        fullCommand = command;
    } else {
        // Note: Elevating a command line process on Windows via popen is restricted.
        fullCommand = command; 
    }
#else
    if (bIsAuthenticated_) {
        fullCommand = "echo '" + CachedPassword_ + "' | sudo -S " + command;
    } else {
        fullCommand = command;
    }
#endif

    std::array<char, 128> buffer;
    outOutput.clear();
    
#if defined(_WIN32)
    FILE* pipe = _popen(fullCommand.c_str(), "r");
#else
    FILE* pipe = popen(fullCommand.c_str(), "r");
#endif

    if (!pipe) return false;
    
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

bool PrivilegeEscalationAgentModule::IsElevated() const {
    std::lock_guard<std::mutex> lock(AuthMutex_);
    if (bIsAuthenticated_) return true;
    return PlatformCheckElevation();
}

Core::EscalationHandle PrivilegeEscalationAgentModule::GetEscalationHandle() {
    std::lock_guard<std::mutex> lock(AuthMutex_);
    Core::EscalationHandle handle;
    if (bIsAuthenticated_) {
        handle.Status = Core::EEscalationStatus::Authenticated;
        handle.Token = CachedPassword_;
    } else if (PlatformCheckElevation()) {
        handle.Status = Core::EEscalationStatus::Authenticated;
    } else {
        handle.Status = Core::EEscalationStatus::Pending;
    }
    return handle;
}

std::string PrivilegeEscalationAgentModule::GetElevatedCommandPrefix() const {
#ifdef _WIN32
    return ""; // Windows doesn't use a simple prefix like sudo for popen
#else
    return "sudo -S ";
#endif
}

std::string PrivilegeEscalationAgentModule::GetEscalationMenuId() const {
    return "escalation_menu";
}

bool PrivilegeEscalationAgentModule::Authenticate(const std::string& credentials) {
    if (PlatformVerifyCredentials(credentials)) {
        std::lock_guard<std::mutex> lock(AuthMutex_);
        CachedPassword_ = credentials;
        bIsAuthenticated_ = true;
        return true;
    }
    return false;
}

void PrivilegeEscalationAgentModule::Deauthenticate() {
    std::lock_guard<std::mutex> lock(AuthMutex_);
    CachedPassword_.clear();
    bIsAuthenticated_ = false;
}

Core::CanvasMenuActionResult PrivilegeEscalationAgentModule::OnMenuAction(const Core::CanvasMenuActionRequest& request) {
    Core::CanvasMenuActionResult result;
    result.Success = true;

    if (request.ActionId == "privilege.action.authenticate") {
        auto it = std::find_if(request.ContextValues.begin(), request.ContextValues.end(), [](const auto& pair) {
            return pair.first == "password" || pair.first == "privilege.password";
        });

        if (it != request.ContextValues.end()) {
            if (Authenticate(it->second)) {
                result.NavigateToMenuId = "menu.back";
            } else {
                result.Success = false;
                result.ErrorMessage = "Authentication failed. Invalid password.";
            }
        }
    }

    return result;
}

bool PrivilegeEscalationAgentModule::PlatformCheckElevation() const {
#ifdef _WIN32
    return IsUserAnAdmin() != FALSE;
#else
    return geteuid() == 0;
#endif
}

bool PrivilegeEscalationAgentModule::PlatformVerifyCredentials(const std::string& password) {
#ifdef _WIN32
    // On Windows, verifying a password without LogonUser is complex.
    // For this implementation, we'll assume it's valid if not empty.
    // A real implementation would use LogonUser or try to spawn a process.
    return !password.empty();
#else
    // On Linux/macOS, try sudo -S -v (validate timestamp)
    std::string cmd = "echo '" + password + "' | sudo -S -v 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return false;

    char buffer[128];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

    int res = pclose(pipe);
    return res == 0;
#endif
}
