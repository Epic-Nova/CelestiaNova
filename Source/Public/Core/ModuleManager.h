#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Core/ModuleLoader.h"

namespace Core {

struct ExtensionDescriptor {
    std::string id;
    std::string name;
    std::string description;
    std::string longDescription;
    std::string file; // filename or relative path to extension library
    std::string version;
    // Build-level dependencies: extensions whose headers/libraries this
    // extension links against at compile time.
    std::vector<std::string> dependencies;
    // Runtime extension dependencies declared by the descriptor itself or by a
    // content pack injected via ContentForge. These are resolved and loaded
    // (topologically) before this extension's StartupModule() is called.
    //
    // Example: an AuthAPI content pack declares
    //   "extensionDependencies": ["rabbitmq-orchestrator"]
    // The ExtensionRegistry will ensure RabbitMQOrchestrator is started
    // (and its credentials sourced via KeyForge) before AuthAPI activates.
    //
    // Chained deps are resolved fully: if ExtA → ExtB → ExtC, load order is
    // ExtC, ExtB, ExtA. Circular deps are rejected at load time.
    std::vector<std::string> extensionDependencies;
    std::vector<std::string> publicIncludePaths;
    bool autostart = true;
    int startupDelayMs = 0; // milliseconds
};

class ModuleManager {
public:
    static ModuleManager& Instance();

    // Discover plugins in `pluginsDir` and load them according to descriptors.
    // This method is non-interactive and will return after scheduling any
    // delayed startups. Returns number of successfully loaded plugins.
    int DiscoverAndLoad(const std::string& pluginsDir = "Extensions");

    // Unloads all loaded plugins.
    void UnloadAll();

    const std::vector<std::unique_ptr<LoadedModule>>& loadedModules() const { return loader_.modules(); }

private:
    ModuleLoader loader_;
    ModuleManager() = default;
};

} // namespace Core
