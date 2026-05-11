#include "ContentForge.h"

#include "Core/NovaLog.h"
#include "Core/ExtensionRegistry.h"
#include "ExtensionSpecific/IPackageManagerAgent.h"

ContentForgeModule::ContentForgeModule() {
    std::lock_guard<std::mutex> lock(ProviderMutex_);
    ContentProviders_ = { "LocalFileSystem", "GitRemote", "S3Static" };
}

ContentForgeModule::~ContentForgeModule() {}

void ContentForgeModule::StartupModule() {
    NOVA_LOG("[ContentForge] StartupModule called", LogType::Log);
}

void ContentForgeModule::ShutdownModule() {
    NOVA_LOG("[ContentForge] ShutdownModule called", LogType::Log);
}

Core::NovaCapabilityDescriptor ContentForgeModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "contentforge";
    descriptor.displayName = "ContentForge";
    descriptor.description = "Content pack and extension asset lifecycle core.";
    descriptor.serviceCapabilities = { "content.fetch", "content.install", "content.list" };
    descriptor.healthEndpoints = { "/api/v1/health/contentforge" };
    descriptor.contentPacks = { "ContentPackRuntime" };
    descriptor.telemetryStreams = { "contentforge.fetch.count", "contentforge.pack.count" };
    descriptor.grafanaDashboards = { "grafana://celestianova/contentforge-delivery" };
    return descriptor;
}

Core::NovaHealthSnapshot ContentForgeModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "ContentForge base module initialized";
    return health;
}

bool ContentForgeModule::Clone(const std::string& url, const std::string& destination, const std::string& branch) {
    return FetchViaGitAgent(url, destination);
}

bool ContentForgeModule::Pull(const std::string& repoPath) {
    NOVA_LOG(("[ContentForge] Pulling latest for " + repoPath).c_str(), LogType::Log);
    return true;
}

bool ContentForgeModule::Push(const std::string& repoPath, const std::string& message) {
    NOVA_LOG(("[ContentForge] Pushing changes for " + repoPath + " with message: " + message).c_str(), LogType::Log);
    return true;
}

std::string ContentForgeModule::GetCurrentBranch(const std::string& repoPath) {
    return "main";
}

bool ContentForgeModule::IsRepo(const std::string& path) {
    return true;
}

bool ContentForgeModule::MountFileSystemPath(const std::string& sourcePath, const std::string& mountTarget) const {
    NOVA_LOG(("[ContentForge] Mounting file system path '" + sourcePath + "' to target '" + mountTarget + "'.").c_str(), LogType::Log);
    return true;
}

bool ContentForgeModule::FetchViaGitAgent(const std::string& repoUrl, const std::string& destination) const {
    NOVA_LOG(("[ContentForge] Fetching content via GitAgent from '" + repoUrl + "' into '" + destination + "'.").c_str(), LogType::Log);
    
    auto& registry = Core::ExtensionRegistry::Instance();
    auto* git = dynamic_cast<Core::ISourceControlAgent*>(registry.GetLoadedExtensionInstance("gitagent"));
    if (git) {
        return git->Clone(repoUrl, destination);
    }
    
    return false;
}

std::vector<std::string> ContentForgeModule::GetContentProviders() const {
    std::lock_guard<std::mutex> lock(ProviderMutex_);
    return ContentProviders_;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, ContentForgeModule)
