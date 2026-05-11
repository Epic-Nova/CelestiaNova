#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/IPrivilegeEscalationAgent.h"
#include "ExtensionSpecific/IMenuActionProvider.h"
#include <string>
#include <mutex>

#ifdef PrivilegeEscalationAgent_EXPORTS
#  define PRIVILEGEESCALATIONAGENT_API NOVA_EXPORT
#else
#  define PRIVILEGEESCALATIONAGENT_API NOVA_IMPORT
#endif

class PRIVILEGEESCALATIONAGENT_API PrivilegeEscalationAgentModule : 
    public IExtensionInterface,
    public Core::IExtensionAgent,
    public Core::IPrivilegeEscalationAgent,
    public Core::IMenuActionProvider {
public:
    PrivilegeEscalationAgentModule();
    ~PrivilegeEscalationAgentModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    // IExtensionAgent Implementation
    std::string GetAgentId() const override { return "privilege-escalation-agent"; }
    std::string GetAgentName() const override { return "Privilege Escalation Agent"; }
    bool IsInstalled() const override { return true; } // Always installed
    bool Install(std::function<void(const std::string&)> onProgress = nullptr) override { return true; }
    bool Uninstall() override { return false; } // Cannot uninstall core capability
    bool RunCommand(const std::string& command, std::string& outOutput) override;
    bool Configure(const std::string& configKey, const std::string& configValue) override { return true; }

    // IPrivilegeEscalationAgent Implementation
    bool IsElevated() const override;
    Core::EscalationHandle GetEscalationHandle() override;
    std::string GetElevatedCommandPrefix() const override;
    std::string GetEscalationMenuId() const override;
    bool Authenticate(const std::string& credentials) override;
    void Deauthenticate() override;

    // IMenuActionProvider Implementation
    Core::CanvasMenuActionResult OnMenuAction(const Core::CanvasMenuActionRequest& request) override;

private:
    std::string CachedPassword_;
    mutable std::mutex AuthMutex_;
    bool bIsAuthenticated_ = false;

    bool PlatformCheckElevation() const;
    bool PlatformVerifyCredentials(const std::string& password);
};

#ifdef PrivilegeEscalationAgent_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(PRIVILEGEESCALATIONAGENT_API, PrivilegeEscalationAgentModule)
#endif
