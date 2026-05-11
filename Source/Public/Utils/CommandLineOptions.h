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
    class NOVA_CORE_API CommandLineOptions
    {
    public:

        static CommandLineOptions* GetSingletonInstance()
        {
            instance_ = instance_ ? instance_ : new CommandLineOptions();
            return instance_;
        }

        // Register an option with its associated behavior
        void RegisterOption(const std::string& name, std::string* valuePtr, std::function<void()> handler)
        {
            optionMapping_[name] = valuePtr;
            optionHandlers_[name] = handler;
        }

        // Register a boolean option
        void RegisterBoolOption(const std::string& name, bool* boolPtr, std::function<void()> handler)
        {
            boolMapping_[name] = boolPtr;
            optionHandlers_[name] = handler;
        }

        // Get the mapping of options to their flags (for string/valued options)
        const std::unordered_map<std::string, std::string*>& GetOptionMapping() const
        {
            return optionMapping_;
        }

        // Get the mapping of boolean options
        const std::unordered_map<std::string, bool*>& GetBoolMapping() const
        {
            return boolMapping_;
        }

        // Execute the behavior for all enabled options
        void ExecuteEnabledOptions() const
        {
            for (const auto& [name, ptr] : boolMapping_) {
                if (ptr && *ptr) {
                    auto it = optionHandlers_.find(name);
                    if (it != optionHandlers_.end()) {
                        it->second();
                    }
                }
            }

            for (const auto& [name, flag] : optionMapping_) {
                if (flag && !flag->empty() && *flag != "false") { 
                    auto it = optionHandlers_.find(name);
                    if (it != optionHandlers_.end()) {
                        it->second();
                    }
                }
            }
        }

        // Check if a specific option is enabled
        bool IsOptionEnabled(const std::string& name) const
        {
            auto itBool = boolMapping_.find(name);
            if (itBool != boolMapping_.end()) {
                return itBool->second && *(itBool->second);
            }

            auto it = optionMapping_.find(name);
            if (it != optionMapping_.end()) {
                return it->second && !it->second->empty() && *(it->second) != "false";
            }
            return false;
        }

        // Check if an option is registered
        bool IsOptionRegistered(const std::string& name) const
        {
            return optionMapping_.find(name) != optionMapping_.end() || boolMapping_.find(name) != boolMapping_.end();
        }

        // Update an option value dynamically
        void SetOptionValue(const std::string& name, const std::string& value)
        {
            auto itBool = boolMapping_.find(name);
            if (itBool != boolMapping_.end() && itBool->second) {
                *(itBool->second) = (value == "true" || value == "1");
                auto handlerIt = optionHandlers_.find(name);
                if (handlerIt != optionHandlers_.end()) {
                    handlerIt->second();
                }
                return;
            }

            auto it = optionMapping_.find(name);
            if (it != optionMapping_.end() && it->second) {
                *(it->second) = value;
                auto handlerIt = optionHandlers_.find(name);
                if (handlerIt != optionHandlers_.end()) {
                    handlerIt->second();
                }
            }
        }

    private:
        std::unordered_map<std::string, std::string*> optionMapping_; // Maps option names to flags
        std::unordered_map<std::string, bool*> boolMapping_; // Maps option names to bool flags
        std::unordered_map<std::string, std::function<void()>> optionHandlers_; // Maps option names to behaviors

        static CommandLineOptions* instance_; // Singleton instance
    };
}