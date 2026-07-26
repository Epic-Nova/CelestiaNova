#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"
#include "ExtensionSpecific/IMenuActionProvider.h"
#include "ExtensionSpecific/IPrivilegeEscalationAgent.h"
#include "ExtensionSpecific/ISignalCoreSurfaces.h"
#include "ExtensionSpecific/IExtensionCliProvider.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include "Core/FTSTicker.h"

#ifdef TerminalAgent_EXPORTS
#  define TERMINALAGENT_API NOVA_EXPORT
#else
#  define TERMINALAGENT_API NOVA_IMPORT
#endif

namespace CoreTerminal {

struct TerminalCommandRequest {
    std::string command;
    std::string workingDirectory;
    std::map<std::string, std::string> environmentOverrides;
    bool bRequireEscalation = false;
};

struct TerminalCommandResult {
    int exitCode = -1;
    std::string stdOut;
    std::string stdErr;
    bool bEscalationRequired = false;
};

// Remote commands deliberately use the operating system SSH agent. Content
// descriptors may reference KeyForge for ownership/auditing, but must never
// carry a private-key path or credential value.
struct RemoteCommandRequest {
    std::string host;
    unsigned short port = 22;
    std::string user;
    std::string knownHostsFile;
    std::string command;
};

struct RemoteDirectoryUploadRequest {
    std::string host;
    unsigned short port = 22;
    std::string user;
    std::string knownHostsFile;
    std::string localDirectory;
    std::string remoteDirectory;
};

enum class CommandStatus {
    Running,
    Completed,
    Failed,
    NotFound
};

struct CommandStatusResult {
    CommandStatus status;
    int exitCode = -1;
};

class ITerminalAgent {
public:
    virtual ~ITerminalAgent() = default;

    virtual TerminalCommandResult ExecuteCommandSync(const TerminalCommandRequest& request) = 0;
    virtual std::string ExecuteCommandAsync(const TerminalCommandRequest& request, std::function<void(TerminalCommandResult)> callback) = 0;
    virtual std::string ExecuteRemoteCommandAsync(const RemoteCommandRequest& request, std::function<void(TerminalCommandResult)> callback) = 0;
    virtual std::string UploadDirectoryAsync(const RemoteDirectoryUploadRequest& request, std::function<void(TerminalCommandResult)> callback) = 0;
    virtual bool StreamCommandOutput(const std::string& commandId, std::function<void(const std::string& output)> onData) = 0;
    virtual CommandStatusResult GetCommandStatus(const std::string& commandId) = 0;
    virtual bool TerminateCommand(const std::string& commandId) = 0;
    virtual bool CheckHostCapability(const std::string& executable) = 0;
};

} // namespace CoreTerminal

class TERMINALAGENT_API TerminalAgentModule : 
    public IExtensionInterface, 
    public Core::INovaCapabilityProvider,
    public CoreTerminal::ITerminalAgent,
    public Core::IMenuActionProvider,
    public Core::IExtensionCliProvider {
public:
    TerminalAgentModule();
    ~TerminalAgentModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    // IExtensionCliProvider Implementation
    std::vector<Core::FExtensionCliArgDescriptor> GetCliArgDescriptors() const override;
    void ApplyCliArgs(const std::vector<Core::FExtensionCliArg>& args) override;

    // ITerminalAgent Implementation
    CoreTerminal::TerminalCommandResult ExecuteCommandSync(const CoreTerminal::TerminalCommandRequest& request) override;
    std::string ExecuteCommandAsync(const CoreTerminal::TerminalCommandRequest& request, std::function<void(CoreTerminal::TerminalCommandResult)> callback) override;
    std::string ExecuteRemoteCommandAsync(const CoreTerminal::RemoteCommandRequest& request, std::function<void(CoreTerminal::TerminalCommandResult)> callback) override;
    std::string UploadDirectoryAsync(const CoreTerminal::RemoteDirectoryUploadRequest& request, std::function<void(CoreTerminal::TerminalCommandResult)> callback) override;
    bool StreamCommandOutput(const std::string& commandId, std::function<void(const std::string& output)> onData) override;
    CoreTerminal::CommandStatusResult GetCommandStatus(const std::string& commandId) override;
    bool TerminateCommand(const std::string& commandId) override;
    bool CheckHostCapability(const std::string& executable) override;
    std::string GetHistory() const;

    // IMenuActionProvider Implementation
    Core::CanvasMenuActionResult OnMenuAction(const Core::CanvasMenuActionRequest& request) override;

private:
    Core::ISignalNotificationBus* ResolveSignalNotificationBus() const;
    // ... history moved to bottom with mutex ...

    struct AsyncProcessContext {
        std::string CommandId;
        std::thread Thread;
        std::atomic<bool> ShouldTerminate{false};
        std::atomic<CoreTerminal::CommandStatus> Status{CoreTerminal::CommandStatus::Running};
        int ExitCode = -1;

        std::mutex BufferMutex;
        std::queue<std::string> OutputBuffer;
        std::string FullOutput;

        std::function<void(CoreTerminal::TerminalCommandResult)> CompletionCallback;
        std::function<void(const std::string&)> StreamCallback;

        Core::FTSTicker::FDelegateHandle TickerHandle;
    };

    mutable std::mutex ProcessMapMutex_;
    std::map<std::string, std::shared_ptr<AsyncProcessContext>> ActiveProcesses_;

    mutable std::mutex HistoryMutex_;
    std::string CommandHistory_;

    Core::IPrivilegeEscalationAgent* GetEscalationAgent() const;
};
