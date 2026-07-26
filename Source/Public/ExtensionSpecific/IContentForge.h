#pragma once

#include <string>
#include <vector>
#include "Core/ModuleAPI.h"
#include "ExtensionSpecific/ISourceControlAgent.h"

namespace Core {

struct LocalContentDescriptor {
    std::string id;
    std::string path;
    // The source declaration stays with the content pack.  `path` is always
    // the resolved local path (a developer path or ContentForge's source
    // cache), so orchestrators never need to know where a pack came from.
    std::string sourceType = "local-path";
    std::string sourceRepository;
    std::string sourceRef;
    std::vector<std::string> sourceAllowedHosts;
    std::string version;
    std::string framework;
    std::string orchestrator;
    std::string composeFile;
    std::string primaryService;
    std::string healthEndpoint;
    std::string manifestPath;
    // Explicit local-development bridge only. ContentForge never copies this
    // file; the owning orchestrator may inject it into a materialized release.
    std::string localEnvironmentFile;
};

/**
 * An immutable, local staging release produced from a content pack.
 *
 * ContentForge materializes source files and non-secret metadata only. Secret
 * resolution, template rendering and command execution deliberately happen in
 * later orchestration stages.
 */
struct LocalContentRelease {
    std::string contentId;
    std::string releaseId;
    std::string releasePath;
    std::string sourcePath;
    std::string manifestPath;
    std::string version;
};

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

    /** Registers content that is already present on the local host. */
    virtual bool RegisterLocalContent(const LocalContentDescriptor& descriptor) = 0;

    /** Returns a locally registered content entry, if one exists. */
    virtual bool ResolveLocalContent(const std::string& contentId, LocalContentDescriptor& outDescriptor) const = 0;

    /** Returns the locally available content packs discovered from ContentForge roots. */
    virtual std::vector<LocalContentDescriptor> ListLocalContent() const = 0;

    /**
     * Stages a content pack under the configured ContentForge runtime root.
     * The optional release id must be a safe path segment; an empty value creates
     * a unique local id. This operation never resolves secrets or executes code.
     */
    virtual bool MaterializeLocalContent(const std::string& contentId,
                                         const std::string& requestedReleaseId,
                                         LocalContentRelease& outRelease) = 0;
};

} // namespace Core
