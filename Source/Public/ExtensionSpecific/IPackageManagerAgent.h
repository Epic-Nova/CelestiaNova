#pragma once

#include "ExtensionSpecific/IExtensionAgent.h"
#include <string>
#include <vector>

namespace Core {

/**
 * Interface for platform-agnostic package management.
 * Dispatches to Brew (macOS), Choco (Windows), or Apt (Linux).
 */
class IPackageManagerAgent : public virtual IExtensionInterface {
public:
    virtual ~IPackageManagerAgent() = default;

    /**
     * Installs a package.
     */
    virtual bool InstallPackage(const std::string& packageName) = 0;

    /**
     * Uninstalls a package.
     */
    virtual bool UninstallPackage(const std::string& packageName) = 0;

    /**
     * Checks if a package is installed.
     */
    virtual bool IsPackageInstalled(const std::string& packageName) = 0;

    /**
     * Updates all packages.
     */
    virtual bool UpdateAll() = 0;

    /**
     * Returns the name of the underlying package manager (e.g., "brew", "choco", "apt").
     */
    virtual std::string GetUnderlyingManagerName() const = 0;
};

} // namespace Core
