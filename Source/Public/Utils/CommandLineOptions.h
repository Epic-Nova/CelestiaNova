#pragma once

#include "NovaMinimal.h"
#include <unordered_map>
#include <string>
#include <functional>

namespace Utils
{
    struct CommandLineOptionsStruct
    {
        bool clearContent = false; // Clear content directory
        bool noRoot = false; // Disable root access
        bool verbose = false; // Enable verbose logging
        bool requestRootForBrew = true; // Request root access for Homebrew
        bool requestRootForPip = true; // Request root access for Pip
        bool requestRootForVenv = true; // Request root access for Virtual Environment
        bool scrollableLogAlwaysVisible = false; // Keep scrollable log always visible
        // When true, suppresses mouse-movement events in the CanvasCore menu loop
        // (disables the "Light Party" cursor-tracking animation).
        // Persisted to Config/app_options.json via OptionsMenu.
        bool disableMousePartyMode = false;
        std::string mkdocsProjectPath = "Rawr, I am a Dinosaur!"; // Path to MkDocs project
    };
    

    /**
     * Class to manage application options dynamically.
     */
    class CommandLineOptions
    {
    public:

        static CommandLineOptions* GetSingletonInstance()
        {
            instance_ = instance_ ? instance_ : new CommandLineOptions();
            return instance_;
        }

        // Register an option with its associated behavior
        void RegisterOption(const std::string& name, std::string* flag, std::function<void()> handler)
        {
            optionMapping_[name] = flag;
            optionHandlers_[name] = handler;
        }

        // Get the mapping of options to their flags
        const std::unordered_map<std::string, std::string*>& GetOptionMapping() const
        {
            return optionMapping_;
        }

        // Execute the behavior for all enabled options
        void ExecuteEnabledOptions() const
        {
            for (const auto& [name, flag] : optionMapping_) {
                if (*flag == "true") { // Assuming flags are stored as "true" or "false"
                    auto it = optionHandlers_.find(name);
                    if (it != optionHandlers_.end()) {
                        it->second(); // Call the associated behavior
                    }
                }
            }
        }

        // Check if a specific option is enabled
        bool IsOptionEnabled(const std::string& name) const
        {
            auto it = optionMapping_.find(name);
            if (it != optionMapping_.end()) {
                return *(it->second) == "true"; // Assuming flags are stored as "true" or "false"
            }
            return false; // Option not found, consider it disabled
        }

    private:
        std::unordered_map<std::string, std::string*> optionMapping_; // Maps option names to flags
        std::unordered_map<std::string, std::function<void()>> optionHandlers_; // Maps option names to behaviors

        static CommandLineOptions* instance_; // Singleton instance
    };
}