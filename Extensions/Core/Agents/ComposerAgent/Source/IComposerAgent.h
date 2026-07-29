#pragma once

#include <string>
#include <vector>
#include "Core/ModuleAPI.h"
#include "ExtensionSpecific/IExtensionAgent.h"

namespace Core {

/**
 * Interface for PHP Composer dependency management.
 */
class IComposerAgent : public virtual IExtensionAgent {
public:
    virtual ~IComposerAgent() = default;

    /**
     * Checks if composer is installed and accessible.
     */
    virtual bool IsComposerInstalled() const = 0;

    /**
     * Installs composer if not present.
     */
    virtual bool InstallComposer() const = 0;

    /**
     * Runs 'composer install' in the specified directory.
     */
    virtual bool InstallDependencies(const std::string& workingDir, bool noDev = false) const = 0;
    virtual bool InstallDependenciesSync(const std::string& workingDir,
                                         bool noDev = false,
                                         bool ignorePhpPlatformRequirement = false,
                                         bool noScripts = false) const = 0;

    /**
     * Runs 'composer update' in the specified directory.
     */
    virtual bool UpdateDependencies(const std::string& workingDir) const = 0;

    /**
     * Validates a composer.json file at the specified path.
     */
    virtual bool ValidateConfig(const std::string& workingDir) const = 0;
};

} // namespace Core
