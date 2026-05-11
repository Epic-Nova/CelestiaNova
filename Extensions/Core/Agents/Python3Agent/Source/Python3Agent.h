#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "IPythonAgent.h"
#include "ExtensionSpecific/IMenuActionProvider.h"
#include "Core/RequirementResolver.h"
#include <string>
#include <functional>
#include <mutex>
#include <vector>

#ifdef Python3Agent_EXPORTS
#  define PYTHON3AGENT_API NOVA_EXPORT
#else
#  define PYTHON3AGENT_API NOVA_IMPORT
#endif

#include "ExtensionSpecific/IExtensionAgent.h"

namespace Core {

class PYTHON3AGENT_API Python3AgentModule : 
    public IExtensionInterface,
    public IPythonAgent,
    public IMenuActionProvider {
public:
    Python3AgentModule();
    ~Python3AgentModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    // IExtensionAgent Implementation
    std::string GetAgentId() const override { return "python3-agent"; }
    std::string GetAgentName() const override { return "Python 3 Agent"; }
    bool IsInstalled() const override;
    bool Install(std::function<void(const std::string&)> onProgress = nullptr) override;
    bool Uninstall() override;
    bool RunCommand(const std::string& command, std::string& outOutput) override;
    bool Configure(const std::string& configKey, const std::string& configValue) override;

    // IPythonAgent Implementation
    bool CreateVirtualEnv(const std::string& path) override;
    bool PipInstall(const std::string& package, const std::string& venvPath = "") override;
    bool RunScript(const std::string& scriptPath, const std::vector<std::string>& args = {}, const std::string& venvPath = "") override;
    std::string GetPythonVersion() override;

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
#ifdef Python3Agent_EXPORTS
    NOVA_EXPORT bool Python3Agent_Resolve(const void* requestPtr, void* resultPtr);
#endif
}

#ifdef Python3Agent_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(PYTHON3AGENT_API, Core::Python3AgentModule)
#endif
