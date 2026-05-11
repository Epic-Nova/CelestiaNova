#pragma once

#include "ExtensionSpecific/IExtensionAgent.h"
#include <string>
#include <vector>

namespace Core {

/**
 * Interface for source control operations.
 * Implemented by GitAgent and other version control providers.
 */
class ISourceControlAgent {
public:
    virtual ~ISourceControlAgent() = default;

    /**
     * Clones/Fetches a repository.
     * @param url Repository URL.
     * @param destination Local path to clone into.
     * @param branch Optional branch or tag name.
     * @return True if successful.
     */
    virtual bool Clone(const std::string& url, const std::string& destination, const std::string& branch = "") = 0;

    /**
     * Pulls latest changes for a repository.
     * @param repoPath Path to the local repository.
     * @return True if successful.
     */
    virtual bool Pull(const std::string& repoPath) = 0;

    /**
     * Commits and pushes changes.
     * @param repoPath Path to the local repository.
     * @param message Commit message.
     * @return True if successful.
     */
    virtual bool Push(const std::string& repoPath, const std::string& message) = 0;

    /**
     * Gets the current branch/ref of a repository.
     */
    virtual std::string GetCurrentBranch(const std::string& repoPath) = 0;

    /**
     * Checks if a directory is a valid repository for this source control provider.
     */
    virtual bool IsRepo(const std::string& path) = 0;
};

} // namespace Core
