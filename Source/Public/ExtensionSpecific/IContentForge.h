#pragma once

#include <string>
#include <vector>
#include "Core/ModuleAPI.h"
#include "ExtensionSpecific/ISourceControlAgent.h"

namespace Core {

/**
 * Interface for content management and asset lifecycle.
 */
class IContentForge : public virtual ISourceControlAgent {
public:
    virtual ~IContentForge() = default;

    /**
     * Mounts a local file system path to a virtual mount target.
     */
    virtual bool MountFileSystemPath(const std::string& sourcePath, const std::string& mountTarget) const = 0;

    /**
     * Fetches content from a remote repository using the internal Git agent.
     */
    virtual bool FetchViaGitAgent(const std::string& repoUrl, const std::string& destination) const = 0;

    /**
     * Returns a list of active content providers managed by the forge.
     */
    virtual std::vector<std::string> GetContentProviders() const = 0;
};

} // namespace Core
