#include "DockerOrchestrator.h"

#include "Core/NovaLog.h"
#include "Core/ExtensionRegistry.h"
#include "TerminalAgent.h"
#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <atomic>
#include <map>
#include <utility>

namespace {

std::atomic<unsigned long long> NextComposeJobSequence{1};

DockerComposeResult ExecuteComposeCommand(const std::string& projectPath,
                                          const std::string& composeFile,
                                          const std::string& command,
                                          std::function<void(const std::string&)> onOutputLine = {}) {
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
    request.onOutputLine = std::move(onOutputLine);
    const auto result = terminal->ExecuteCommandSync(request);
    deployment.exitCode = result.exitCode;
    deployment.output = result.stdOut;
    deployment.succeeded = result.exitCode == 0;
    return deployment;
}

class ComposeProgressReporter {
public:
    explicit ComposeProgressReporter(DockerComposeProgressCallback callback)
        : callback_(std::move(callback)) {}

    void BeginPull() { Emit(85, "Docker image download is starting"); }
    void BeginServices() { Emit(97, "Docker images are ready; creating services"); }
    void FinishServices() { Emit(99, "Docker services were created; checking startup"); }

    void ConsumePullLine(const std::string& line) {
        const auto event = nlohmann::json::parse(line, nullptr, false);
        if (!event.is_discarded() && event.is_object()) {
            // Docker Compose v2 emits current/total at the top level.  Keep
            // the nested Docker Engine form as a compatibility fallback.
            auto total = event.value("total", static_cast<unsigned long long>(0));
            auto current = event.value("current", static_cast<unsigned long long>(0));
            if (total == 0 && event.contains("progressDetail") && event["progressDetail"].is_object()) {
                const auto& details = event["progressDetail"];
                total = details.value("total", static_cast<unsigned long long>(0));
                current = details.value("current", static_cast<unsigned long long>(0));
            }
            const auto layer = event.value("id", std::string{});
            if (!layer.empty() && total > 0) {
                const auto boundedCurrent = std::min(current, total);
                layers_[layer] = {boundedCurrent, total};
                unsigned long long allCurrent = 0;
                unsigned long long allTotal = 0;
                for (const auto& [_, progress] : layers_) {
                    allCurrent += progress.first;
                    allTotal += progress.second;
                }
                if (allTotal > 0) {
                    const int downloadPercent = 85 + static_cast<int>((allCurrent * 11) / allTotal);
                    Emit(std::clamp(downloadPercent, 85, 96),
                         "Docker image download: " + std::to_string((allCurrent * 100) / allTotal) + "% of known layer bytes");
                    return;
                }
            }
            const auto status = event.value("text", event.value("status", std::string{}));
            if (status == "Pull complete" || status == "Download complete" || status == "Already exists") {
                ++completedLayers_;
                Emit(std::min(96, 86 + completedLayers_),
                     "Docker image layer completed (" + std::to_string(completedLayers_) + ")");
                return;
            }
        }
        ConsumeTextLine(line, true);
    }

    void ConsumeServiceLine(const std::string& line) { ConsumeTextLine(line, false); }

private:
    void ConsumeTextLine(std::string line, bool pulling) {
        std::transform(line.begin(), line.end(), line.begin(), [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        if (line.find("pulling") != std::string::npos || line.find("downloading") != std::string::npos) {
            Emit(87, "Docker is downloading image layers");
        } else if (line.find("extracting") != std::string::npos) {
            Emit(92, "Docker is extracting image layers");
        } else if (line.find("pull complete") != std::string::npos || line.find("already exists") != std::string::npos) {
            ++completedLayers_;
            Emit(std::min(96, 86 + completedLayers_),
                 "Docker image layer completed (" + std::to_string(completedLayers_) + ")");
        } else if (!pulling && (line.find("building") != std::string::npos || line.find("build") != std::string::npos)) {
            Emit(98, "Docker is building the local application image");
        } else if (!pulling && (line.find("creating") != std::string::npos || line.find("created") != std::string::npos)) {
            Emit(98, "Docker is creating application containers");
        } else if (!pulling && (line.find("starting") != std::string::npos || line.find("started") != std::string::npos || line.find("running") != std::string::npos)) {
            Emit(99, "Docker is starting application containers");
        }
    }

    void Emit(int percent, const std::string& activity) {
        if (!callback_) return;
        percent = std::max(lastPercent_, percent);
        if (percent == lastPercent_ && activity == lastActivity_) return;
        lastPercent_ = percent;
        lastActivity_ = activity;
        callback_(percent, activity);
    }

    DockerComposeProgressCallback callback_;
    std::map<std::string, std::pair<unsigned long long, unsigned long long>> layers_;
    int completedLayers_ = 0;
    int lastPercent_ = 0;
    std::string lastActivity_;
};

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
    return StartComposeWithProgress(projectPath, {}, composeFile);
}

DockerComposeResult DockerOrchestratorModule::StartComposeWithProgress(const std::string& projectPath,
                                                                        DockerComposeProgressCallback onProgress,
                                                                        const std::string& composeFile) const {
    ComposeProgressReporter reporter(std::move(onProgress));
    reporter.BeginPull();

    // Compose JSON progress exposes the byte totals of every registry image
    // layer. Buildable services (such as Laravel Sail's local app image) must
    // be skipped here: they have no registry reference and are built in the
    // next phase.
    const auto pull = ExecuteComposeCommand(projectPath, composeFile, "--progress json pull --ignore-buildable",
        [&reporter](const std::string& line) { reporter.ConsumePullLine(line); });
    if (!pull.succeeded) {
        return pull;
    }

    reporter.BeginServices();
    const auto start = ExecuteComposeCommand(projectPath, composeFile, "up -d --build",
        [&reporter](const std::string& line) { reporter.ConsumeServiceLine(line); });
    if (start.succeeded) {
        reporter.FinishServices();
    }
    return start;
}

std::vector<Core::FExtensionCliArgDescriptor> DockerOrchestratorModule::GetCliArgDescriptors() const {
    return {{"docker-bootstrap", "Install the local Docker runtime through the restricted DockerOrchestrator helper.", false}};
}

void DockerOrchestratorModule::ApplyCliArgs(const std::vector<Core::FExtensionCliArg>& args) {
    for (const auto& argument : args) {
        if (argument.Flag != "docker-bootstrap") continue;
        const auto result = BootstrapLocalRuntime();
        const auto message = "[DockerOrchestrator] Local Docker bootstrap " +
            std::string(result.succeeded ? "succeeded. " : "failed. ") + result.output;
        NOVA_LOG(message.c_str(), result.succeeded ? LogType::Log : LogType::Error);
    }
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

DockerComposeResult DockerOrchestratorModule::BootstrapLocalRuntime() const {
    DockerComposeResult result;
    auto* terminal = dynamic_cast<CoreTerminal::ITerminalAgent*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("terminalagent"));
    if (!terminal) {
        result.output = "TerminalAgent is not loaded.";
        return result;
    }
    CoreTerminal::TerminalCommandRequest availability;
    availability.command = "docker --version && docker compose version";
    const auto availabilityResult = terminal->ExecuteCommandSync(availability);
    if (availabilityResult.exitCode == 0) {
        result.succeeded = true;
        result.exitCode = 0;
        result.output = "Docker runtime is already available.";
        return result;
    }
    // This is intentionally a fixed path with no caller-provided input. The
    // service installer owns both the root-owned script and its sudoers rule.
    CoreTerminal::TerminalCommandRequest request;
    request.command = "sudo -n /usr/local/lib/celestianova/bootstrap-docker";
    const auto terminalResult = terminal->ExecuteCommandSync(request);
    result.exitCode = terminalResult.exitCode;
    result.output = terminalResult.stdOut.empty() ? terminalResult.stdErr : terminalResult.stdOut;
    result.succeeded = terminalResult.exitCode == 0;
    return result;
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
