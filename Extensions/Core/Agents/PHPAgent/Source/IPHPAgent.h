#pragma once

#include <string>
#include <vector>
#include <map>
#include "Core/ModuleAPI.h"
#include "ExtensionSpecific/IExtensionAgent.h"

namespace Core {

/**
 * Interface for PHP runtime management.
 */
class IPHPAgent : public virtual IExtensionAgent {
public:
    virtual ~IPHPAgent() = default;

    /**
     * Checks if PHP is installed and returns version string.
     */
    virtual std::string GetPHPVersion() const = 0;

    /**
     * Checks if a specific PHP extension is loaded.
     */
    virtual bool IsExtensionLoaded(const std::string& extName) const = 0;

    /**
     * Runs a PHP script.
     */
    virtual bool RunScript(const std::string& scriptPath, const std::vector<std::string>& args = {}) const = 0;

    /**
     * Gets the path to the php.ini being used.
     */
    virtual std::string GetIniPath() const = 0;
};

} // namespace Core
