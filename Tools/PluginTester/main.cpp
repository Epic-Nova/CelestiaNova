#include "Core/ModuleLoader.h"
#include "Core/NovaLog.h"
#include <thread>
#include <chrono>

int main(int argc, char** argv) {
    if (argc < 2) {
        NOVA_LOG("Usage: PluginTester <path-to-plugin-lib>", LogType::Log);
        return 1;
    }

    const char* path = argv[1];
    Core::ModuleLoader loader;
    auto mod = loader.LoadModule(path);
    if (!mod) {
        NOVA_LOG((std::string("Failed to load plugin: ") + path).c_str(), LogType::Error);
        return 2;
    }
    NOVA_LOG((std::string("Plugin loaded: ") + mod->path).c_str(), LogType::Log);
    if (mod->instance) {
        mod->instance->StartupModule();
        // minimal wait to allow the plugin to do work; non-interactive by default
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        mod->instance->ShutdownModule();
    }

    loader.UnloadModule(mod);
    NOVA_LOG("Plugin unloaded.", LogType::Log);
    return 0;
}
