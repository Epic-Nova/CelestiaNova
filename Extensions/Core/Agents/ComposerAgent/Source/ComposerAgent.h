#pragma once

#include "IComposerAgent.h"
#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/IMenuActionProvider.h"
#include "Core/RequirementResolver.h"
#include <mutex>
#include <vector>

namespace Core {

class ComposerAgentModule : public IExtensionInterface, 
                           public IComposerAgent,
                           public IMenuActionProvider {
public:
    ComposerAgentModule();
    ~ComposerAgentModule() override;

    // IExtensionInterface
    void StartupModule() override;
    void ShutdownModule() override;

    // IExtensionAgent
    std::string GetAgentId() const override { return "composeragent"; }
    std::string GetAgentName() const override { return "ComposerAgent"; }
    bool IsInstalled() const override;
    bool Install(std::function<void(const std::string&)> onProgress = nullptr) override;
    bool Uninstall() override;
    bool RunCommand(const std::string& command, std::string& outOutput) override;
    bool Configure(const std::string& configKey, const std::string& configValue) override;

    // IComposerAgent
    bool IsComposerInstalled() const override;
    bool InstallComposer() const override;
    bool InstallDependencies(const std::string& workingDir, bool noDev = false) const override;
    bool UpdateDependencies(const std::string& workingDir) const override;
    bool ValidateConfig(const std::string& workingDir) const override;

    // IMenuActionProvider
    CanvasMenuActionResult OnMenuAction(const CanvasMenuActionRequest& request) override;

    // Requirement Resolver
    Core::RequirementResolver::CoreRequirementResolveResult Resolve(const Core::RequirementResolver::CoreRequirementResolveRequest& request);

private:
    std::string GetComposerCommand(const std::string& workingDir) const;
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
#ifdef ComposerAgent_EXPORTS
    NOVA_EXPORT bool ComposerAgent_Resolve(const void* requestPtr, void* resultPtr);
#endif
}

#ifdef ComposerAgent_EXPORTS
#  define COMPOSERAGENT_API NOVA_EXPORT
#else
#  define COMPOSERAGENT_API NOVA_IMPORT
#endif

NOVA_DECLARE_MODULE_FACTORY(COMPOSERAGENT_API, Core::ComposerAgentModule)
