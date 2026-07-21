#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include <functional>
#include <string>

struct DockerComposeResult {
    bool succeeded = false;
    int exitCode = -1;
    std::string output;
};

class IDockerOrchestrator {
public:
    virtual ~IDockerOrchestrator() = default;
    virtual DockerComposeResult StartCompose(const std::string& projectPath, const std::string& composeFile = "compose.yaml") const = 0;
    virtual DockerComposeResult StopCompose(const std::string& projectPath, const std::string& composeFile = "compose.yaml") const = 0;
    virtual bool StartComposeAsync(const std::string& projectPath, std::function<void(DockerComposeResult)> onComplete, const std::string& composeFile = "compose.yaml") const = 0;
    virtual bool StopComposeAsync(const std::string& projectPath, std::function<void(DockerComposeResult)> onComplete, const std::string& composeFile = "compose.yaml") const = 0;
    virtual bool IsComposeServiceRunning(const std::string& projectPath, const std::string& serviceName, const std::string& composeFile = "compose.yaml") const = 0;
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
};

#ifdef DockerOrchestrator_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(DOCKERORCHESTRATOR_API, DockerOrchestratorModule)
#endif

