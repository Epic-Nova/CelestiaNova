#pragma once

#include <string>
#include <functional>
#include <vector>
#include "Core/ModuleAPI.h"

namespace Core {

/**
 * Base interface for all capability agents (Git, Python, Package Manager, etc.)
 * Provides common functionality for installation, configuration, and command execution.
 */
class IExtensionAgent {
public:
    virtual ~IExtensionAgent() = default;

    /**
     * Returns the unique identifier of the agent.
     */
    virtual std::string GetAgentId() const = 0;

    /**
     * Returns the human-readable name of the agent.
     */
    virtual std::string GetAgentName() const = 0;

    /**
     * Checks if the underlying tool/software is installed on the system.
     */
    virtual bool IsInstalled() const = 0;

    /**
     * Attempts to install the underlying tool/software.
     * @param onProgress Callback for installation progress messages.
     * @return True if installation was successful or already installed.
     */
    virtual bool Install(std::function<void(const std::string&)> onProgress = nullptr) = 0;

    /**
     * Attempts to uninstall the underlying tool/software.
     * @return True if uninstallation was successful.
     */
    virtual bool Uninstall() = 0;

    /**
     * Executes a command through the agent.
     * @param command The command to execute.
     * @param outOutput The standard output/error of the command.
     * @return True if the command executed successfully (return code 0).
     */
    virtual bool RunCommand(const std::string& command, std::string& outOutput) = 0;

    /**
     * Configures the agent or the underlying tool with specific settings.
     * @param configKey The key of the configuration setting.
     * @param configValue The value to set.
     * @return True if configuration was successful.
     */
    virtual bool Configure(const std::string& configKey, const std::string& configValue) = 0;
};

} // namespace Core
