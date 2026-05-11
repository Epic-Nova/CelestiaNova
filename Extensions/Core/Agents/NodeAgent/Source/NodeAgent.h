#pragma once

#include "INodeAgent.h"
#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/IMenuActionProvider.h"
#include "Core/RequirementResolver.h"
#include <mutex>
#include <vector>

namespace Core {

class NodeAgentModule : public IExtensionInterface, 
                        public INodeAgent,
                        public IMenuActionProvider {
public:
    NodeAgentModule();
    ~NodeAgentModule() override;

    // IExtensionInterface
    void StartupModule() override;
    void ShutdownModule() override;

    // IExtensionAgent
    std::string GetAgentId() const override { return "nodeagent"; }
    std::string GetAgentName() const override { return "NodeAgent"; }
    bool IsInstalled() const override;
    bool Install(std::function<void(const std::string&)> onProgress = nullptr) override;
    bool Uninstall() override;
    bool RunCommand(const std::string& command, std::string& outOutput) override;
    bool Configure(const std::string& configKey, const std::string& configValue) override;

    // INodeAgent
    bool IsNodeInstalled() const override;
    bool RunInstall(const std::string& workingDir) const override;
    bool RunScript(const std::string& workingDir, const std::string& scriptName) const override;
    std::string DetectPackageManager(const std::string& workingDir) const override;

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
#ifdef NodeAgent_EXPORTS
    NOVA_EXPORT bool NodeAgent_Resolve(const void* requestPtr, void* resultPtr);
#endif
}

#ifdef NodeAgent_EXPORTS
#  define NODEAGENT_API NOVA_EXPORT
#else
#  define NODEAGENT_API NOVA_IMPORT
#endif

NOVA_DECLARE_MODULE_FACTORY(NODEAGENT_API, Core::NodeAgentModule)
