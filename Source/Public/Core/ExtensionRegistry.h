#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Core/ModuleLoader.h"
#include "Core/ModuleManager.h"
#include "Core/NovaMinimal.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"

namespace Core {

struct ExtensionStatusSnapshot {
    std::string id;
    std::string name;
    std::string description;
    std::string version;
    std::string descriptorPath;
    bool loaded = false;
    bool exposesCapabilityProvider = false;
    std::string healthStatus;
    std::string healthSummary;
    std::string healthUpdatedAtUtc;
};

class NOVA_CORE_API ExtensionRegistry {
public:
    static ExtensionRegistry& Instance();

    // Discover extension descriptors (JSON files) under `extensionsDir`.
    // Returns the number of descriptor files registered.
    int Discover(const std::string& extensionsDir = "Extensions");

    // Register an extension from a descriptor file. Returns true on success.
    bool RegisterDescriptor(const std::string& descriptorPath);

    // List registered extension descriptors.
    std::vector<ExtensionDescriptor> ListExtensionDescriptors() const;

    bool HasExtension(const std::string& id) const;
    const ExtensionDescriptor* GetExtensionDescriptor(const std::string& id) const;
    std::string GetExtensionDescriptorPath(const std::string& id) const;
    IExtensionInterface* GetLoadedExtensionInstance(const std::string& id) const;
    void* GetLoadedExtensionSymbol(const std::string& id, const std::string& symbolName) const;
    // Returns true if the extension with given id is currently loaded.
    bool IsExtensionLoaded(const std::string& id) const;

    // Build a runtime status snapshot suitable for service APIs and frontend
    // status dashboards.
    std::vector<ExtensionStatusSnapshot> BuildExtensionStatusSnapshot() const;

    // Serialize status snapshot to JSON for HTTP API consumers.
    std::string BuildExtensionStatusSnapshotJson() const;

    // Load/unload by id. Loading will open the shared library and call
    // CreateModuleInstance(), then call StartupModule() if available.
    bool LoadExtensionById(const std::string& id);
    bool UnloadExtensionById(const std::string& id);

    // Unload all loaded extensions.
    void UnloadAllExtensions();

private:
    ExtensionRegistry() = default;
    ~ExtensionRegistry();

    struct Entry {
        ExtensionDescriptor desc;
        std::string descriptorPath;
        std::string libFullPath;
        enum State { Unloaded, Loaded, Failed } state = Unloaded;
        LoadedModule* loadedModule = nullptr;
    };

    Entry* FindEntry(const std::string& id);

    std::vector<std::unique_ptr<Entry>> entries_;
    ModuleLoader loader_;
};

} // namespace Core
