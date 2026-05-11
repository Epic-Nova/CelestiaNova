#pragma once

#include "ExtensionSpecific/IExtensionAgent.h"
#include <string>
#include <vector>

namespace Core {

/**
 * Interface for Python and Pip operations.
 */
class IPythonAgent : public virtual IExtensionAgent {
public:
    virtual ~IPythonAgent() = default;

    /**
     * Creates a virtual environment.
     * @param path Path where the venv should be created.
     * @return True if successful.
     */
    virtual bool CreateVirtualEnv(const std::string& path) = 0;

    /**
     * Installs a pip package.
     * @param package Package name or requirements.txt path.
     * @param venvPath Optional path to a virtual environment.
     * @return True if successful.
     */
    virtual bool PipInstall(const std::string& package, const std::string& venvPath = "") = 0;

    /**
     * Runs a python script.
     * @param scriptPath Path to the script.
     * @param args Command line arguments.
     * @param venvPath Optional path to a virtual environment.
     * @return True if successful.
     */
    virtual bool RunScript(const std::string& scriptPath, const std::vector<std::string>& args = {}, const std::string& venvPath = "") = 0;

    /**
     * Gets the python version.
     */
    virtual std::string GetPythonVersion() = 0;
};

} // namespace Core
