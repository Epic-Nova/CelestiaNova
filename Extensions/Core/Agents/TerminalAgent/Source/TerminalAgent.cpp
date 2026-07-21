#include "TerminalAgent.h"

#include "Core/NovaLog.h"
#include "Core/ExtensionRegistry.h"

#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <array>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace {

bool ApplyWorkingDirectory(const CoreTerminal::TerminalCommandRequest& request, std::string& command) {
    if (request.workingDirectory.empty()) {
        return true;
    }

    if (request.workingDirectory.find_first_of("\"\r\n") != std::string::npos) {
        return false;
    }

#ifdef _WIN32
    command = "cd /d \"" + request.workingDirectory + "\" && " + command;
#else
    command = "cd \"" + request.workingDirectory + "\" && " + command;
#endif
    return true;
}

} // namespace

TerminalAgentModule::TerminalAgentModule() {}
TerminalAgentModule::~TerminalAgentModule() {}

void TerminalAgentModule::StartupModule() {
    NOVA_LOG("[TerminalAgent] StartupModule called. Terminal execution engine ready.", LogType::Log);
}

std::vector<Core::FExtensionCliArgDescriptor> TerminalAgentModule::GetCliArgDescriptors() const {
    std::vector<Core::FExtensionCliArgDescriptor> descriptors;
    
    Core::FExtensionCliArgDescriptor cmdArg;
    cmdArg.Flag = "exec-command";
    cmdArg.Description = "Execute a specific command on startup in the background.";
    cmdArg.RequiresValue = true;
    descriptors.push_back(std::move(cmdArg));

    Core::FExtensionCliArgDescriptor autoEscalate;
    autoEscalate.Flag = "auto-escalate";
    autoEscalate.Description = "Automatically attempt to elevate commands if sudo is detected.";
    autoEscalate.RequiresValue = false;
    descriptors.push_back(std::move(autoEscalate));

    return descriptors;
}

void TerminalAgentModule::ApplyCliArgs(const std::vector<Core::FExtensionCliArg>& args) {
    for (const auto& arg : args) {
        if (arg.Flag == "exec-command" && !arg.Value.empty()) {
            NOVA_LOG(("[TerminalAgent] CLI: Received startup command: " + arg.Value).c_str(), LogType::Log);
            CoreTerminal::TerminalCommandRequest req;
            req.command = arg.Value;
            ExecuteCommandAsync(req, nullptr);
        } else if (arg.Flag == "auto-escalate") {
            NOVA_LOG("[TerminalAgent] CLI: Auto-escalate enabled via command line.", LogType::Log);
            // In a real implementation, we'd set a member variable here
        }
    }
}

void TerminalAgentModule::ShutdownModule() {
    NOVA_LOG("[TerminalAgent] ShutdownModule called. Cleaning up active terminal processes.", LogType::Log);
    std::lock_guard<std::mutex> lock(ProcessMapMutex_);
    for (auto& pair : ActiveProcesses_) {
        if (pair.second) {
            pair.second->ShouldTerminate = true;
            if (pair.second->Thread.joinable()) {
                pair.second->Thread.detach(); // Or join, but might block
            }
            if (pair.second->TickerHandle.IsValid()) {
                Core::FTSTicker::GetCoreTicker().RemoveTicker(pair.second->TickerHandle);
            }
        }
    }
    ActiveProcesses_.clear();
}



Core::IPrivilegeEscalationAgent* TerminalAgentModule::GetEscalationAgent() const {
    auto& registry = Core::ExtensionRegistry::Instance();
    auto* instance = registry.GetLoadedExtensionInstance("privilegeescalationagent");
    return dynamic_cast<Core::IPrivilegeEscalationAgent*>(instance);
}

CoreTerminal::TerminalCommandResult TerminalAgentModule::ExecuteCommandSync(const CoreTerminal::TerminalCommandRequest& request) {
    NOVA_LOG(("[TerminalAgent] Executing sync command: " + request.command).c_str(), LogType::Log);
    
    CoreTerminal::TerminalCommandResult result;
    result.exitCode = -1;

    std::string commandToExecute = request.command;
    std::string password;

    if (!ApplyWorkingDirectory(request, commandToExecute)) {
        result.stdErr = "Working directory contains unsupported characters.";
        return result;
    }

    if (request.bRequireEscalation) {
        auto* escalationAgent = GetEscalationAgent();
        if (escalationAgent) {
            bool bElevated = escalationAgent->IsElevated();
            auto handle = escalationAgent->GetEscalationHandle();
            
            if (!bElevated && handle.Status == Core::EEscalationStatus::Authenticated) {
                password = handle.Token;
            } else if (!bElevated) {
                result.stdErr = "Elevation required but session not authenticated.";
                result.bEscalationRequired = true;
                return result;
            }
            
            std::string prefix = escalationAgent->GetElevatedCommandPrefix();
            if (!prefix.empty() && commandToExecute.find(prefix) != 0) {
                commandToExecute = prefix + commandToExecute;
            }
        }
    }

    std::string fullCommand = commandToExecute + " 2>&1";
    
    // Support passing password to sudo -S
    if (!password.empty() && commandToExecute.find("sudo -S") != std::string::npos) {
        fullCommand = "echo '" + password + "' | " + fullCommand;
    }

    std::array<char, 128> buffer;
    std::string output;
    
    FILE* pipe = POPEN(fullCommand.c_str(), "r");
    if (!pipe) {
        result.stdErr = "popen() failed!";
        return result;
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    int returnCode = PCLOSE(pipe);
    result.exitCode = returnCode;
    result.stdOut = output;

    return result;
}

Core::CanvasMenuActionResult TerminalAgentModule::OnMenuAction(const Core::CanvasMenuActionRequest& request) {
    Core::CanvasMenuActionResult actionResult;
    
    std::string logMsg = "[TerminalAgent] OnMenuAction: Menu=" + request.MenuId + ", Action=" + request.ActionId;
    NOVA_LOG(logMsg.c_str(), LogType::Log);

    if (request.ActionId == "invoke") {
        // Handle direct invocation from the extensions list
        auto it = request.ContextValues.find("selectedExtension");
        if (it != request.ContextValues.end() && it->second == "terminalagent") {
            actionResult.Success = true;
            actionResult.NavigateToMenuId = "terminal_ui";
            return actionResult;
        }
    }

    if (request.MenuId == "terminal_ui" && request.ActionId == "execute") {
        std::string commandInput;
        auto it = request.ContextValues.find("commandInput");
        if (it != request.ContextValues.end()) {
            commandInput = it->second;
        }

        if (commandInput.empty()) {
            actionResult.Success = false;
            actionResult.ErrorMessage = "Command input is empty.";
            return actionResult;
        }

        bool bNeedsEscalation = (commandInput.find("sudo") == 0);
        
        CoreTerminal::TerminalCommandRequest cmdReq;
        cmdReq.command = commandInput;
        cmdReq.bRequireEscalation = bNeedsEscalation;

        if (bNeedsEscalation) {
            auto* escalationAgent = GetEscalationAgent();
            if (escalationAgent && !escalationAgent->IsElevated()) {
                auto handle = escalationAgent->GetEscalationHandle();
                if (handle.Status != Core::EEscalationStatus::Authenticated) {
                    actionResult.Success = true;
                    actionResult.NavigateToMenuId = escalationAgent->GetEscalationMenuId();
                    return actionResult;
                }
            }
        }

        // Start async command
        ExecuteCommandAsync(cmdReq, [this, commandInput](CoreTerminal::TerminalCommandResult result) {
            NOVA_LOG(("[TerminalAgent] Command completed: " + commandInput).c_str(), LogType::Log);
        });

        // Immediately update history with the command line
        {
            std::lock_guard<std::mutex> lock(HistoryMutex_);
            CommandHistory_ += "\n> " + commandInput + "\n";
        }
        
        // Clear input field immediately
        actionResult.ConfigUpdates["commandInput"] = "";
        {
            std::lock_guard<std::mutex> lock(HistoryMutex_);
            actionResult.ConfigUpdates["commandHistory"] = CommandHistory_;
        }

        actionResult.Success = true;
    } else {
        // If we didn't handle it, don't return failure here if it might be for another provider
        // but since we are specifically checking IDs, we can return false if it matched none of our patterns
        // but only if it's definitely targeted at us.
        // NOTE: Specific extensions might add their own sub-menus or interaction surfaces here later.
        actionResult.Success = false;
        actionResult.ErrorMessage = "Unhandled action or menu.";
    }

    return actionResult;
}

std::string TerminalAgentModule::ExecuteCommandAsync(const CoreTerminal::TerminalCommandRequest& request, std::function<void(CoreTerminal::TerminalCommandResult)> callback) {
    NOVA_LOG(("[TerminalAgent] Executing async command: " + request.command).c_str(), LogType::Log);
    
    std::string commandId = "cmd-" + std::to_string(rand());
    
    auto context = std::make_shared<AsyncProcessContext>();
    context->CommandId = commandId;
    context->CompletionCallback = callback;
    
    std::string commandToExecute = request.command;
    std::string password;

    if (!ApplyWorkingDirectory(request, commandToExecute)) {
        if (callback) {
            CoreTerminal::TerminalCommandResult result;
            result.stdErr = "Working directory contains unsupported characters.";
            callback(result);
        }
        return "";
    }

    if (request.bRequireEscalation) {
        auto* escalationAgent = GetEscalationAgent();
        if (escalationAgent) {
            bool bElevated = escalationAgent->IsElevated();
            auto handle = escalationAgent->GetEscalationHandle();
            
            if (!bElevated && handle.Status == Core::EEscalationStatus::Authenticated) {
                password = handle.Token;
            } else if (!bElevated) {
                if (callback) {
                    CoreTerminal::TerminalCommandResult res;
                    res.exitCode = -1;
                    res.stdErr = "Elevation required but session not authenticated.";
                    res.bEscalationRequired = true;
                    callback(res);
                }
                return "";
            }
            
            std::string prefix = escalationAgent->GetElevatedCommandPrefix();
            if (!prefix.empty() && commandToExecute.find(prefix) != 0) {
                commandToExecute = prefix + commandToExecute;
            }
        }
    }

    std::string fullCommand = commandToExecute + " 2>&1";
    if (!password.empty() && commandToExecute.find("sudo -S") != std::string::npos) {
        fullCommand = "echo '" + password + "' | " + fullCommand;
    }
    
    // Background execution thread
    context->Thread = std::thread([context, fullCommand, this]() {
        std::array<char, 256> buffer;
        FILE* pipe = POPEN(fullCommand.c_str(), "r");
        
        if (!pipe) {
            context->ExitCode = -1;
            std::lock_guard<std::mutex> lock(context->BufferMutex);
            context->OutputBuffer.push("popen() failed!\n");
            context->FullOutput += "popen() failed!\n";
            context->Status = CoreTerminal::CommandStatus::Failed;
            return;
        }

        while (!context->ShouldTerminate) {
            size_t bytesRead = fread(buffer.data(), 1, buffer.size() - 1, pipe);
            if (bytesRead == 0) {
                if (feof(pipe) || ferror(pipe)) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            buffer[bytesRead] = '\0';
            std::string chunk(buffer.data(), bytesRead);
            
            {
                std::lock_guard<std::mutex> lock(context->BufferMutex);
                context->OutputBuffer.push(chunk);
                context->FullOutput += chunk;
            }

            // Update global history and trigger UI refresh
            {
                std::lock_guard<std::mutex> lock(this->HistoryMutex_);
                this->CommandHistory_ += chunk;
                
                // Limit history size to prevent memory issues
                if (this->CommandHistory_.size() > 50000) {
                    this->CommandHistory_ = this->CommandHistory_.substr(this->CommandHistory_.size() - 45000);
                }
            }

            // Send signal to refresh CanvasCore UI
            auto* signalBus = ResolveSignalNotificationBus();
            if (signalBus) {
                Core::SignalNotification sig;
                sig.Channel = Core::SignalChannels::UiControl;
                sig.Title = Core::SignalTitles::ForceRefresh;
                sig.SourceExtensionId = "terminalagent";
                signalBus->PublishSignalNotification(sig);
            }
        }

        int returnCode = PCLOSE(pipe);
        if (!context->ShouldTerminate) {
            context->ExitCode = returnCode;
            context->Status = CoreTerminal::CommandStatus::Completed;
        } else {
            context->ExitCode = -1;
            context->Status = CoreTerminal::CommandStatus::Failed;
        }
    });
    context->Thread.detach();

    // Register Ticker to poll results onto main thread
    context->TickerHandle = Core::FTSTicker::GetCoreTicker().AddTicker([this, context](float /*dt*/) -> bool {
        std::queue<std::string> pendingOutput;
        CoreTerminal::CommandStatus currentStatus = context->Status.load();

        {
            std::lock_guard<std::mutex> lock(context->BufferMutex);
            std::swap(pendingOutput, context->OutputBuffer);
        }

        // Dispatch streaming output
        if (context->StreamCallback) {
            while (!pendingOutput.empty()) {
                context->StreamCallback(pendingOutput.front());
                pendingOutput.pop();
            }
        }

        // Check if finished
        if (currentStatus == CoreTerminal::CommandStatus::Completed || currentStatus == CoreTerminal::CommandStatus::Failed) {
            if (context->CompletionCallback) {
                CoreTerminal::TerminalCommandResult res;
                res.exitCode = context->ExitCode;
                std::lock_guard<std::mutex> lock(context->BufferMutex);
                res.stdOut = context->FullOutput;
                context->CompletionCallback(res);
            }
            
            // Clean up thread
            if (context->Thread.joinable()) {
                context->Thread.detach(); // Detach since we can't join inside the ticker callback safely without potential deadlocks if the thread is still exiting
            }

            // Remove from active processes
            std::lock_guard<std::mutex> mapLock(ProcessMapMutex_);
            ActiveProcesses_.erase(context->CommandId);

            return false; // Remove ticker
        }

        return true; // Keep ticking
    }, 0.1f); // Check every 100ms

    {
        std::lock_guard<std::mutex> lock(ProcessMapMutex_);
        ActiveProcesses_[commandId] = context;
    }

    return commandId;
}

bool TerminalAgentModule::StreamCommandOutput(const std::string& commandId, std::function<void(const std::string& output)> onData) {
    NOVA_LOG(("[TerminalAgent] Attached stream listener to command: " + commandId).c_str(), LogType::Log);
    
    std::lock_guard<std::mutex> lock(ProcessMapMutex_);
    auto it = ActiveProcesses_.find(commandId);
    if (it != ActiveProcesses_.end()) {
        it->second->StreamCallback = onData;
        return true;
    }
    return false;
}

CoreTerminal::CommandStatusResult TerminalAgentModule::GetCommandStatus(const std::string& commandId) {
    CoreTerminal::CommandStatusResult status;
    status.status = CoreTerminal::CommandStatus::NotFound;
    status.exitCode = -1;

    std::lock_guard<std::mutex> lock(ProcessMapMutex_);
    auto it = ActiveProcesses_.find(commandId);
    if (it != ActiveProcesses_.end()) {
        status.status = it->second->Status.load();
        status.exitCode = it->second->ExitCode;
    }
    return status;
}

bool TerminalAgentModule::TerminateCommand(const std::string& commandId) {
    NOVA_LOG(("[TerminalAgent] Terminating command: " + commandId).c_str(), LogType::Warning);
    
    std::lock_guard<std::mutex> lock(ProcessMapMutex_);
    auto it = ActiveProcesses_.find(commandId);
    if (it != ActiveProcesses_.end()) {
        it->second->ShouldTerminate = true;
        return true;
    }
    return false;
}

std::string TerminalAgentModule::GetHistory() const {
    std::lock_guard<std::mutex> lock(HistoryMutex_);
    return CommandHistory_;
}

Core::ISignalNotificationBus* TerminalAgentModule::ResolveSignalNotificationBus() const {
    auto& registry = Core::ExtensionRegistry::Instance();
    const auto descriptors = registry.ListExtensionDescriptors();

    for (const auto& descriptor : descriptors) {
        auto* instance = registry.GetLoadedExtensionInstance(descriptor.id);
        if (!instance) {
            continue;
        }

        auto* bus = dynamic_cast<Core::ISignalNotificationBus*>(instance);
        if (bus) {
            return bus;
        }
    }

    return nullptr;
}

bool TerminalAgentModule::CheckHostCapability(const std::string& executable) {
    NOVA_LOG(("[TerminalAgent] Checking host capability for: " + executable).c_str(), LogType::Log);
    // Stub implementation - assume true for demo
    return true;
}

#include "Core/RequirementResolver.h"

extern "C" NOVA_EXPORT bool TerminalAgent_ResolveRequirement(const void* requestPtr, void* resultPtr) {
    auto* registry = &Core::ExtensionRegistry::Instance();
    auto* instance = registry->GetLoadedExtensionInstance("terminalagent");
    auto* agent = dynamic_cast<TerminalAgentModule*>(instance);

    if (!agent) {
        return false;
    }

    return Core::RequirementResolver::DispatchResolveRequest(requestPtr, resultPtr, [&](const Core::RequirementResolver::CoreRequirementResolveRequest& request) {
        Core::RequirementResolver::CoreRequirementResolveResult result;
        
        if (request.RequirementKey == "terminal.history") {
            result.Success = true;
            Core::RequirementResolver::CoreRequirementResolvedOption option;
            option.Value = agent->GetHistory();
            option.Label = "Terminal History";
            result.Options.push_back(std::move(option));
        } else {
            result.Success = false;
            result.ErrorCode = "UnsupportedKey";
            result.ErrorMessage = "TerminalAgent does not support requirement key: " + request.RequirementKey;
        }

        return result;
    });
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, TerminalAgentModule)
