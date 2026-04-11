#pragma once

#include "NovaMinimal.h"
#include "NovaPlatforms.h"
#include <thread>
#include <chrono>

/**
 * Base class for helper utilities.
 */
namespace Core::Helpers
{
    class BaseHelper
    {
    public:
        BaseHelper() = default;

        virtual BaseHelper* GetHelperInstance() = 0;
        virtual bool Initialize() = 0;
        virtual void Shutdown() = 0;

        bool IsInitialized() const { return bIsInitialized; }
        std::string GetName() const { return Name; }
        std::string GetDescription() const { return Description; }

        virtual void Execute(std::function<bool()> callback) = 0;
        virtual void Reset() = 0;
        virtual void Abort() = 0;
        virtual bool HasMetRequirements() const { return true; } // Default implementation, can be overridden
        bool IsRunning() const { return bIsRunning; }

        // Register an empty install callback
        void RegisterEmptyInstallCallback(const std::string& callbackId) {
            if (InstallCallbackFunctions.find(callbackId) == InstallCallbackFunctions.end()) {
                InstallCallbackFunctions[callbackId] = [](const std::string& msg) {
                    // Default empty callback
                };
            }
        }

        // Set or replace an install callback function
        void SetInstallCallbackFunction(const std::string& callbackId, std::function<void(const std::string&)> callback) {
            if (callback) {
                InstallCallbackFunctions[callbackId] = callback;
            }
        }

        // Fires the install callback with a message
        void FireInstallCallback(const std::string& message) {
            if (InstallCallbackFunctions.find(message) != InstallCallbackFunctions.end()) {
                InstallCallbackFunctions[message](message);
            }
        }

        // Execute command with progress tracking and blocking
        virtual bool ExecuteCommandBlocking(const std::string& command) {
            return RunCommandWithProgress(command);
        }

        // Wait for completion flag
        void WaitForCompletion() {
            while (bIsRunning) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

    protected:
        // Common command execution with progress
        virtual bool RunCommandWithProgress(const std::string& command) {
            FireInstallCallback("Executing: " + command);
            
            // Add output redirection for non-verbose mode
            std::string fullCommand = command;
            #ifndef VERBOSE_MODE
            fullCommand += " > /dev/null 2>&1";
            #endif
            
            FILE* pipe = popen(fullCommand.c_str(), "r");
            if (!pipe) {
                FireInstallCallback("Failed to execute command");
                return false;
            }

            char buffer[128];
            std::string result = "";
            int progressCounter = 0;
            
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result += buffer;
                if (++progressCounter % 20 == 0) {
                    FireInstallCallback("Command running... (" + std::to_string(progressCounter/20) + "s)");
                }
            }

            int returnCode = pclose(pipe);
            
            if (returnCode == 0) {
                FireInstallCallback("Command completed successfully");
                return true;
            } else {
                FireInstallCallback("Command failed with return code: " + std::to_string(returnCode));
                return false;
            }
        }

        BaseHelper(std::string NewName, std::string NewDescription)
            : Name(std::move(NewName)), Description(std::move(NewDescription)) {};
        virtual ~BaseHelper() = default;

        // Member variables
        bool bIsInitialized = false;
        bool bIsRunning = false;
        std::string Name;
        std::string Description;
        
        // This callback is used by the installation process itself
        std::map<std::string, std::function<void(const std::string&)>> InstallCallbackFunctions;
    };  
}