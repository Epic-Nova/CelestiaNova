#include "DockerOrchestrator.h"

#include "Core/NovaLog.h"
#include "Core/ExtensionRegistry.h"
#include "TerminalAgent.h"

#include <filesystem>

namespace {

DockerComposeResult ExecuteComposeCommand(const std::string& projectPath,
                                          const std::string& composeFile,
                                          const std::string& command) {
    DockerComposeResult deployment;
    if (composeFile != "compose.yaml" && composeFile != "docker-compose.yml") {
        deployment.output = "Unsupported Compose filename.";
        return deployment;
    }

    std::error_code error;
    const auto path = std::filesystem::weakly_canonical(projectPath, error);
    if (error || !std::filesystem::is_directory(path) || !std::filesystem::is_regular_file(path / composeFile)) {
        deployment.output = "Compose project path or compose file is invalid.";
        return deployment;
    }

    const auto pathString = path.string();
    if (pathString.find_first_of("\"\r\n") != std::string::npos) {
        deployment.output = "Compose project path contains unsupported characters.";
        return deployment;
    }

    auto* terminal = dynamic_cast<CoreTerminal::ITerminalAgent*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminal) {
        deployment.output = "TerminalAgent is not loaded.";
        return deployment;
    }

    CoreTerminal::TerminalCommandRequest request;
    request.workingDirectory = pathString;
    request.command = "docker compose -f " + composeFile + " " + command;
    const auto result = terminal->ExecuteCommandSync(request);
    deployment.exitCode = result.exitCode;
    deployment.output = result.stdOut;
    deployment.succeeded = result.exitCode == 0;
    return deployment;
}

bool ExecuteComposeCommandAsync(const std::string& projectPath,
                                const std::string& composeFile,
                                const std::string& command,
                                std::function<void(DockerComposeResult)> onComplete) {
    DockerComposeResult validation;
    if (composeFile != "compose.yaml" && composeFile != "docker-compose.yml") {
        validation.output = "Unsupported Compose filename.";
    } else {
        std::error_code error;
        const auto path = std::filesystem::weakly_canonical(projectPath, error);
        if (error || !std::filesystem::is_directory(path) || !std::filesystem::is_regular_file(path / composeFile)) {
            validation.output = "Compose project path or compose file is invalid.";
        } else {
            const auto pathString = path.string();
            if (pathString.find_first_of("\"\r\n") != std::string::npos) {
                validation.output = "Compose project path contains unsupported characters.";
            } else {
                auto* terminal = dynamic_cast<CoreTerminal::ITerminalAgent*>(
                    Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
                if (!terminal) {
                    validation.output = "TerminalAgent is not loaded.";
                } else {
                    CoreTerminal::TerminalCommandRequest request;
                    request.workingDirectory = pathString;
                    request.command = "docker compose -f " + composeFile + " " + command;
                    const auto commandId = terminal->ExecuteCommandAsync(request, [onComplete = std::move(onComplete)](CoreTerminal::TerminalCommandResult result) {
                        DockerComposeResult deployment;
                        deployment.exitCode = result.exitCode;
                        deployment.output = result.stdOut;
                        deployment.succeeded = result.exitCode == 0;
                        if (onComplete) {
                            onComplete(std::move(deployment));
                        }
                    });
                    return !commandId.empty();
                }
            }
        }
    }

    if (onComplete) {
        onComplete(std::move(validation));
    }
    return false;
}

} // namespace

DockerOrchestratorModule::DockerOrchestratorModule() {}
DockerOrchestratorModule::~DockerOrchestratorModule() {}

void DockerOrchestratorModule::StartupModule() {
    NOVA_LOG("[DockerOrchestrator] StartupModule called. Docker Compose runtime ready.", LogType::Log);
}

void DockerOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[DockerOrchestrator] ShutdownModule called", LogType::Log);
}

DockerComposeResult DockerOrchestratorModule::StartCompose(const std::string& projectPath, const std::string& composeFile) const {
    return ExecuteComposeCommand(projectPath, composeFile, "up -d");
}

DockerComposeResult DockerOrchestratorModule::StopCompose(const std::string& projectPath, const std::string& composeFile) const {
    return ExecuteComposeCommand(projectPath, composeFile, "stop");
}

bool DockerOrchestratorModule::StartComposeAsync(const std::string& projectPath,
                                                 std::function<void(DockerComposeResult)> onComplete,
                                                 const std::string& composeFile) const {
    return ExecuteComposeCommandAsync(projectPath, composeFile, "up -d", std::move(onComplete));
}

bool DockerOrchestratorModule::StopComposeAsync(const std::string& projectPath,
                                                std::function<void(DockerComposeResult)> onComplete,
                                                const std::string& composeFile) const {
    return ExecuteComposeCommandAsync(projectPath, composeFile, "stop", std::move(onComplete));
}

bool DockerOrchestratorModule::IsComposeServiceRunning(const std::string& projectPath,
                                                       const std::string& serviceName,
                                                       const std::string& composeFile) const {
    if (serviceName.empty() || serviceName.find_first_of(" \"\r\n") != std::string::npos) {
        return false;
    }

    const auto result = ExecuteComposeCommand(projectPath, composeFile, "ps --status running --services");
    if (!result.succeeded) {
        return false;
    }

    return result.output.find(serviceName) != std::string::npos;
}
