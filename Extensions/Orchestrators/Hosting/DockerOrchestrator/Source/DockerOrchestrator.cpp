#include "DockerOrchestrator.h"

#include "Core/NovaLog.h"
#include "Core/ExtensionRegistry.h"
#include "TerminalAgent.h"

#include <filesystem>
#include <atomic>
#include <utility>

namespace {

std::atomic<unsigned long long> NextComposeJobSequence{1};

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

std::string ComposeCommandFor(DockerComposeJobAction action) {
    switch (action) {
        case DockerComposeJobAction::Start: return "up -d";
        case DockerComposeJobAction::Stop: return "stop";
        case DockerComposeJobAction::Status: return "ps --format json";
        case DockerComposeJobAction::Logs: return "logs --tail 200";
    }
    return {};
}

bool ResolveComposeProject(const std::string& projectPath, const std::string& composeFile,
                           std::filesystem::path& path, std::string& errorText) {
    if (composeFile != "compose.yaml" && composeFile != "docker-compose.yml") {
        errorText = "Unsupported Compose filename.";
        return false;
    }
    std::error_code error;
    path = std::filesystem::weakly_canonical(projectPath, error);
    if (error || !std::filesystem::is_directory(path) || !std::filesystem::is_regular_file(path / composeFile)) {
        errorText = "Compose project path or compose file is invalid.";
        return false;
    }
    if (path.string().find_first_of("\"\r\n") != std::string::npos) {
        errorText = "Compose project path contains unsupported characters.";
        return false;
    }
    return true;
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

DockerComposeResult DockerOrchestratorModule::ValidateCompose(const std::string& projectPath,
                                                              const std::string& composeFile) const {
    return ExecuteComposeCommand(projectPath, composeFile, "config -q");
}

DockerComposeJob DockerOrchestratorModule::SubmitComposeJob(DockerComposeJobAction action,
                                                            const std::string& projectPath,
                                                            const std::string& composeFile) {
    DockerComposeJob job;
    job.action = action;
    const auto command = ComposeCommandFor(action);
    if (command.empty()) {
        job.state = DockerComposeJobState::Failed;
        job.output = "Unsupported Docker Compose job action.";
        return job;
    }

    std::filesystem::path path;
    if (!ResolveComposeProject(projectPath, composeFile, path, job.output)) {
        job.state = DockerComposeJobState::Failed;
        return job;
    }

    const auto validation = ValidateCompose(path.string(), composeFile);
    if (!validation.succeeded) {
        job.state = DockerComposeJobState::Failed;
        job.exitCode = validation.exitCode;
        job.output = "Compose validation failed: " + validation.output;
        return job;
    }

    auto* terminal = dynamic_cast<CoreTerminal::ITerminalAgent*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminal) {
        job.state = DockerComposeJobState::Failed;
        job.output = "TerminalAgent is not loaded.";
        return job;
    }

    CoreTerminal::TerminalCommandRequest request;
    request.workingDirectory = path.string();
    request.command = "docker compose -f " + composeFile + " " + command;
    job.id = "docker-compose-" + std::to_string(NextComposeJobSequence.fetch_add(1));
    job.state = DockerComposeJobState::Accepted;
    {
        std::lock_guard<std::mutex> lock(composeJobsMutex_);
        composeJobs_[job.id] = job;
    }

    const auto commandId = terminal->ExecuteCommandAsync(request, [this, jobId = job.id](CoreTerminal::TerminalCommandResult result) {
        CompleteComposeJob(jobId, std::move(result));
    });
    if (commandId.empty()) {
        std::lock_guard<std::mutex> lock(composeJobsMutex_);
        auto& failed = composeJobs_[job.id];
        failed.state = DockerComposeJobState::Failed;
        failed.output = "TerminalAgent did not accept the Compose command.";
        return failed;
    }
    {
        std::lock_guard<std::mutex> lock(composeJobsMutex_);
        auto& running = composeJobs_[job.id];
        if (running.state == DockerComposeJobState::Accepted) {
            running.state = DockerComposeJobState::Running;
        }
    }
    // Return the acknowledgement; subsequent reads expose Running or the terminal result.
    return job;
}

DockerComposeJob DockerOrchestratorModule::GetComposeJob(const std::string& jobId) const {
    std::lock_guard<std::mutex> lock(composeJobsMutex_);
    const auto found = composeJobs_.find(jobId);
    if (found == composeJobs_.end()) {
        DockerComposeJob missing;
        missing.id = jobId;
        return missing;
    }
    return found->second;
}

void DockerOrchestratorModule::CompleteComposeJob(const std::string& jobId, CoreTerminal::TerminalCommandResult result) {
    std::lock_guard<std::mutex> lock(composeJobsMutex_);
    const auto found = composeJobs_.find(jobId);
    if (found == composeJobs_.end()) return;
    found->second.exitCode = result.exitCode;
    found->second.output = result.stdOut.empty() ? result.stdErr : result.stdOut;
    found->second.state = result.exitCode == 0 ? DockerComposeJobState::Succeeded : DockerComposeJobState::Failed;
}

std::string DockerOrchestratorModule::BootstrapRemoteAsync(const DockerRemoteTarget& target,
                                                           std::function<void(DockerComposeResult)> onComplete) {
    auto* terminal = dynamic_cast<CoreTerminal::ITerminalAgent*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminal) {
        if (onComplete) onComplete({false, -1, "TerminalAgent is not loaded."});
        return {};
    }
    CoreTerminal::RemoteCommandRequest request;
    request.host = target.host;
    request.port = target.port;
    request.user = target.user;
    request.knownHostsFile = target.knownHostsFile;
    request.command = "sudo -n apt-get update && sudo -n apt-get install -y docker.io docker-compose-v2 && sudo -n systemctl enable --now docker";
    return terminal->ExecuteRemoteCommandAsync(request, [onComplete = std::move(onComplete)](CoreTerminal::TerminalCommandResult terminalResult) {
        if (onComplete) onComplete({terminalResult.exitCode == 0, terminalResult.exitCode,
                                   terminalResult.stdOut.empty() ? terminalResult.stdErr : terminalResult.stdOut});
    });
}

std::string DockerOrchestratorModule::QueryRemoteComposeAsync(const DockerRemoteTarget& target,
                                                              const std::string& releasePath,
                                                              std::function<void(DockerComposeResult)> onComplete) {
    if (releasePath.empty() || releasePath.front() != '/' || releasePath.find("..") != std::string::npos ||
        releasePath.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789/_-.") != std::string::npos) {
        if (onComplete) onComplete({false, -1, "Remote Compose path is invalid."});
        return {};
    }
    auto* terminal = dynamic_cast<CoreTerminal::ITerminalAgent*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminal) {
        if (onComplete) onComplete({false, -1, "TerminalAgent is not loaded."});
        return {};
    }
    CoreTerminal::RemoteCommandRequest request;
    request.host = target.host;
    request.port = target.port;
    request.user = target.user;
    request.knownHostsFile = target.knownHostsFile;
    request.command = "if [ -d " + releasePath + " ]; then cd " + releasePath + " && docker compose ps; else echo no-active-release; exit 3; fi";
    return terminal->ExecuteRemoteCommandAsync(request, [onComplete = std::move(onComplete)](CoreTerminal::TerminalCommandResult terminalResult) {
        if (onComplete) onComplete({terminalResult.exitCode == 0, terminalResult.exitCode,
                                   terminalResult.stdOut.empty() ? terminalResult.stdErr : terminalResult.stdOut});
    });
}
