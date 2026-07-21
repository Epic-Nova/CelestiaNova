#include "ContentForge.h"

#include "Core/NovaLog.h"
#include "Core/ExtensionRegistry.h"
#include "ExtensionSpecific/IPackageManagerAgent.h"
#include <filesystem>
#include <fstream>
#include <json.hpp>

ContentForgeModule::ContentForgeModule() {
    std::lock_guard<std::mutex> lock(ProviderMutex_);
    ContentProviders_ = { "LocalFileSystem", "GitRemote", "S3Static" };
}

ContentForgeModule::~ContentForgeModule() {}

void ContentForgeModule::StartupModule() {
    NOVA_LOG("[ContentForge] StartupModule called", LogType::Log);

    const auto manifestPath = std::filesystem::current_path() / "Content" / "ContentPacks" / "AuthApiLocal.json";
    std::ifstream manifestFile(manifestPath);
    if (!manifestFile) {
        return;
    }

    try {
        nlohmann::json manifest;
        manifestFile >> manifest;
        if (manifest.value("source", nlohmann::json::object()).value("type", "") != "local-path") {
            return;
        }

        Core::LocalContentDescriptor content;
        content.id = manifest.value("id", "");
        content.version = manifest.value("version", "");
        content.path = (std::filesystem::current_path() / manifest["source"].value("path", "")).string();
        RegisterLocalContent(content);
    } catch (const std::exception& error) {
        NOVA_LOG((std::string("[ContentForge] Failed to read local content manifest: ") + error.what()).c_str(), LogType::Warning);
    }
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

bool ContentForgeModule::RegisterLocalContent(const Core::LocalContentDescriptor& descriptor) {
    if (descriptor.id.empty() || descriptor.path.empty()) {
        NOVA_LOG("[ContentForge] Local content registration requires an id and path.", LogType::Warning);
        return false;
    }

    std::error_code error;
    const auto path = std::filesystem::weakly_canonical(descriptor.path, error);
    if (error || !std::filesystem::is_directory(path)) {
        NOVA_LOG(("[ContentForge] Local content path is not a directory: " + descriptor.path).c_str(), LogType::Warning);
        return false;
    }

    auto normalized = descriptor;
    normalized.path = path.string();
    std::lock_guard<std::mutex> lock(ProviderMutex_);
    LocalContent_[normalized.id] = std::move(normalized);
    NOVA_LOG(("[ContentForge] Registered local content: " + descriptor.id).c_str(), LogType::Log);
    return true;
}

bool ContentForgeModule::ResolveLocalContent(const std::string& contentId, Core::LocalContentDescriptor& outDescriptor) const {
    std::lock_guard<std::mutex> lock(ProviderMutex_);
    const auto iterator = LocalContent_.find(contentId);
    if (iterator == LocalContent_.end()) {
        return false;
    }

    outDescriptor = iterator->second;
    return true;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, ContentForgeModule)
