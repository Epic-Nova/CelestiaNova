#pragma once

#include "Core/ModuleAPI.h"
#include "IPHPAgent.h"
#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/IMenuActionProvider.h"
#include "Core/RequirementResolver.h"
#include <mutex>
#include <vector>
#include <functional>

namespace Core {

class PHPAgentModule : public IExtensionInterface, 
                       public IPHPAgent,
                       public IMenuActionProvider {
public:
    PHPAgentModule();
    ~PHPAgentModule() override;

    // IExtensionInterface
    void StartupModule() override;
    void ShutdownModule() override;

    // IExtensionAgent
    std::string GetAgentId() const override { return "phpagent"; }
    std::string GetAgentName() const override { return "PHPAgent"; }

    // IPHPAgent
    std::string GetPHPVersion() const override;
    bool IsExtensionLoaded(const std::string& extName) const override;
    bool RunScript(const std::string& scriptPath, const std::vector<std::string>& args = {}) const override;
    std::string GetIniPath() const override;

    // IExtensionAgent (required by IPHPAgent)
    bool IsInstalled() const override;
    bool Install(std::function<void(const std::string&)> onProgress = nullptr) override;
    bool Uninstall() override;
    bool RunCommand(const std::string& command, std::string& outOutput) override;
    bool Configure(const std::string& configKey, const std::string& configValue) override;

    // IMenuActionProvider
    CanvasMenuActionResult OnMenuAction(const CanvasMenuActionRequest& request) override;

    // Requirement Resolver
    Core::RequirementResolver::CoreRequirementResolveResult Resolve(const Core::RequirementResolver::CoreRequirementResolveRequest& request);

private:
    void AddLog(const std::string& message);

    struct LogEntry {
        std::string Timestamp;
        std::string Message;
    };

    mutable std::mutex ProgressMutex_;
    std::vector<LogEntry> Logs_;
};

} // namespace Core

extern "C" {
#ifdef PHPAgent_EXPORTS
    NOVA_EXPORT bool PHPAgent_Resolve(const void* requestPtr, void* resultPtr);
#endif
#ifdef PHPAgent_EXPORTS
#  define PHPAGENT_API NOVA_EXPORT
#else
#  define PHPAGENT_API NOVA_IMPORT
#endif

NOVA_DECLARE_MODULE_FACTORY(PHPAGENT_API, Core::PHPAgentModule)
}
