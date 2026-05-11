#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/ISourceControlAgent.h"
#include "ExtensionSpecific/IMenuActionProvider.h"
#include "Core/RequirementResolver.h"
#include <mutex>
#include <vector>
#ifdef GitAgent_EXPORTS
#  define GITAGENT_API NOVA_EXPORT
#else
#  define GITAGENT_API NOVA_IMPORT
#endif

namespace Core {

class GITAGENT_API GitAgentModule : 
    public IExtensionInterface,
    public IExtensionAgent,
    public ISourceControlAgent,
    public IMenuActionProvider {
public:
    GitAgentModule();
    ~GitAgentModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    // IExtensionAgent Implementation
    std::string GetAgentId() const override { return "git-agent"; }
    std::string GetAgentName() const override { return "Git Agent"; }
    bool IsInstalled() const override;
    bool Install(std::function<void(const std::string&)> onProgress = nullptr) override;
    bool Uninstall() override;
    bool RunCommand(const std::string& command, std::string& outOutput) override;
    bool Configure(const std::string& configKey, const std::string& configValue) override;

    // ISourceControlAgent Implementation
    bool Clone(const std::string& url, const std::string& destination, const std::string& branch = "") override;
    bool Pull(const std::string& repoPath) override;
    bool Push(const std::string& repoPath, const std::string& message) override;
    std::string GetCurrentBranch(const std::string& repoPath) override;
    bool IsRepo(const std::string& path) override;

    // IMenuActionProvider Implementation
    CanvasMenuActionResult OnMenuAction(const CanvasMenuActionRequest& request) override;

    // Requirement Resolver Implementation
    Core::RequirementResolver::CoreRequirementResolveResult Resolve(const Core::RequirementResolver::CoreRequirementResolveRequest& request);

private:
    void AddLog(const std::string& message);

    struct LogEntry {
        std::string Timestamp;
        std::string Message;
    };

    mutable std::mutex ProgressMutex_;
    std::vector<LogEntry> Logs_;

    bool ExecuteCommandInternal(const std::string& command, std::string& outOutput);
};

} // namespace Core

extern "C" {
#ifdef GitAgent_EXPORTS
    NOVA_EXPORT bool GitAgent_Resolve(const void* requestPtr, void* resultPtr);
#endif
}

#ifdef GitAgent_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(GITAGENT_API, Core::GitAgentModule)
#endif
