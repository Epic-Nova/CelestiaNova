#pragma once

#include <string>
#include <vector>
#include "Core/ModuleAPI.h"
#include "ExtensionSpecific/IExtensionAgent.h"

namespace Core {

/**
 * Interface for Node.js and Javascript environment management.
 */
class INodeAgent : public virtual IExtensionAgent {
public:
    virtual ~INodeAgent() = default;

    /**
     * Checks if Node.js is installed.
     */
    virtual bool IsNodeInstalled() const = 0;

    /**
     * Runs package manager install (npm/pnpm/yarn).
     */
    virtual bool RunInstall(const std::string& workingDir) const = 0;

    /**
     * Runs a script defined in package.json.
     */
    virtual bool RunScript(const std::string& workingDir, const std::string& scriptName) const = 0;

    /**
     * Detects the preferred package manager for the directory.
     */
    virtual std::string DetectPackageManager(const std::string& workingDir) const = 0;
};

} // namespace Core
