#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "TerminalAgent.h"
#include <functional>
#include <map>
#include <mutex>
#include <string>

struct DockerComposeResult {
    bool succeeded = false;
    int exitCode = -1;
    std::string output;
};

enum class DockerComposeJobAction {
    Start,
    Stop,
    Status,
    Logs
};

enum class DockerComposeJobState {
    Accepted,
    Running,
    Succeeded,
    Failed,
    NotFound
};

struct DockerComposeJob {
    std::string id;
    DockerComposeJobAction action = DockerComposeJobAction::Status;
    DockerComposeJobState state = DockerComposeJobState::NotFound;
    int exitCode = -1;
    std::string output;
};

struct DockerRemoteTarget {
    std::string host;
    unsigned short port = 22;
    std::string user;
    std::string knownHostsFile;
};

class IDockerOrchestrator {
public:
    virtual ~IDockerOrchestrator() = default;
    virtual DockerComposeResult StartCompose(const std::string& projectPath, const std::string& composeFile = "compose.yaml") const = 0;
    virtual DockerComposeResult StopCompose(const std::string& projectPath, const std::string& composeFile = "compose.yaml") const = 0;
    virtual bool StartComposeAsync(const std::string& projectPath, std::function<void(DockerComposeResult)> onComplete, const std::string& composeFile = "compose.yaml") const = 0;
    virtual bool StopComposeAsync(const std::string& projectPath, std::function<void(DockerComposeResult)> onComplete, const std::string& composeFile = "compose.yaml") const = 0;
    virtual bool IsComposeServiceRunning(const std::string& projectPath, const std::string& serviceName, const std::string& composeFile = "compose.yaml") const = 0;
    virtual DockerComposeResult ValidateCompose(const std::string& projectPath, const std::string& composeFile = "compose.yaml") const = 0;
    virtual DockerComposeJob SubmitComposeJob(DockerComposeJobAction action, const std::string& projectPath, const std::string& composeFile = "compose.yaml") = 0;
    virtual DockerComposeJob GetComposeJob(const std::string& jobId) const = 0;
    virtual std::string BootstrapRemoteAsync(const DockerRemoteTarget& target, std::function<void(DockerComposeResult)> onComplete) = 0;
    virtual std::string QueryRemoteComposeAsync(const DockerRemoteTarget& target, const std::string& releasePath, std::function<void(DockerComposeResult)> onComplete) = 0;
};

#ifdef DockerOrchestrator_EXPORTS
#  define DOCKERORCHESTRATOR_API NOVA_EXPORT
#else
#  define DOCKERORCHESTRATOR_API NOVA_IMPORT
#endif

class DOCKERORCHESTRATOR_API DockerOrchestratorModule : public IExtensionInterface, public IDockerOrchestrator {
public:
    DockerOrchestratorModule();
    ~DockerOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    DockerComposeResult StartCompose(const std::string& projectPath, const std::string& composeFile = "compose.yaml") const override;
    DockerComposeResult StopCompose(const std::string& projectPath, const std::string& composeFile = "compose.yaml") const override;
    bool StartComposeAsync(const std::string& projectPath, std::function<void(DockerComposeResult)> onComplete, const std::string& composeFile = "compose.yaml") const override;
    bool StopComposeAsync(const std::string& projectPath, std::function<void(DockerComposeResult)> onComplete, const std::string& composeFile = "compose.yaml") const override;
    bool IsComposeServiceRunning(const std::string& projectPath, const std::string& serviceName, const std::string& composeFile = "compose.yaml") const override;
    DockerComposeResult ValidateCompose(const std::string& projectPath, const std::string& composeFile = "compose.yaml") const override;
    DockerComposeJob SubmitComposeJob(DockerComposeJobAction action, const std::string& projectPath, const std::string& composeFile = "compose.yaml") override;
    DockerComposeJob GetComposeJob(const std::string& jobId) const override;
    std::string BootstrapRemoteAsync(const DockerRemoteTarget& target, std::function<void(DockerComposeResult)> onComplete) override;
    std::string QueryRemoteComposeAsync(const DockerRemoteTarget& target, const std::string& releasePath, std::function<void(DockerComposeResult)> onComplete) override;

private:
    void CompleteComposeJob(const std::string& jobId, CoreTerminal::TerminalCommandResult result);
    mutable std::mutex composeJobsMutex_;
    std::map<std::string, DockerComposeJob> composeJobs_;
};

#ifdef DockerOrchestrator_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(DOCKERORCHESTRATOR_API, DockerOrchestratorModule)
#endif

