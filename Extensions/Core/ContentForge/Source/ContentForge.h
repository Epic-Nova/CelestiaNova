#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"
#include "ExtensionSpecific/IContentForge.h"
#include "ExtensionSpecific/IRemoteControl.h"
#include <mutex>
#include <map>
#include <vector>
#include <string>

#ifdef ContentForge_EXPORTS
#  define CONTENTFORGE_API NOVA_EXPORT
#else
#  define CONTENTFORGE_API NOVA_IMPORT
#endif

class CONTENTFORGE_API ContentForgeModule : 
    public IExtensionInterface, 
    public Core::INovaCapabilityProvider,
    public Core::IContentForge,
    public Core::IRemoteControlTargetResolver {
public:
    ContentForgeModule();
    ~ContentForgeModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;

    // ISourceControlAgent Implementation
    bool Clone(const std::string& url, const std::string& destination, const std::string& branch = "") override;
    bool Pull(const std::string& repoPath) override;
    bool Push(const std::string& repoPath, const std::string& message) override;
    std::string GetCurrentBranch(const std::string& repoPath) override;
    bool IsRepo(const std::string& path) override;

    // IContentForge Implementation
    bool MountFileSystemPath(const std::string& sourcePath, const std::string& mountTarget) const override;
    bool FetchViaGitAgent(const std::string& repoUrl, const std::string& destination) const override;
    std::vector<std::string> GetContentProviders() const override;
    bool RegisterLocalContent(const Core::LocalContentDescriptor& descriptor) override;
    bool ResolveLocalContent(const std::string& contentId, Core::LocalContentDescriptor& outDescriptor) const override;
    std::vector<Core::LocalContentDescriptor> ListLocalContent() const override;
    bool MaterializeLocalContent(const std::string& contentId,
                                 const std::string& requestedReleaseId,
                                 Core::LocalContentRelease& outRelease) override;
    bool ResolveAllowlistedRemoteControlRequest(const std::string& targetId,
                                                const std::string& commandId,
                                                std::string& outUrl,
                                                std::string& outMethod,
                                                std::string& outRequiredCapability,
                                                std::string& outError) const override;

private:
    mutable std::mutex ProviderMutex_;
    std::vector<std::string> ContentProviders_;
    std::map<std::string, Core::LocalContentDescriptor> LocalContent_;
    std::string ApplicationRoot_;
};
