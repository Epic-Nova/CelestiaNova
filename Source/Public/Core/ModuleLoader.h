#pragma once

#include <string>
#include <memory>
#include <vector>
#include "Core/IExtensionInterface.h"
#include "Core/SharedLibrary.h"
#include "Core/ModuleAPI.h"

namespace Core {

struct LoadedModule {
    std::string path;
    std::unique_ptr<SharedLibrary> lib; // platform-agnostic library wrapper
    IExtensionInterface* instance = nullptr;
    using DestroyFn = void (*)(IExtensionInterface*);
    DestroyFn destroyFn = nullptr;
};

class NOVA_CORE_API ModuleLoader {
public:
    ModuleLoader() = default;
    ModuleLoader(const ModuleLoader&) = delete;
    ModuleLoader& operator=(const ModuleLoader&) = delete;
    ~ModuleLoader();

    // Loads the shared library at `path` and calls its CreateModuleInstance
    // factory. Returns the LoadedModule on success, or nullptr on failure.
    LoadedModule* LoadModule(const std::string& path);

    // Unloads the module (calls destroy factory if present and closes the library).
    void UnloadModule(LoadedModule* module);

    const std::vector<std::unique_ptr<LoadedModule>>& modules() const { return modules_; }

private:
    std::vector<std::unique_ptr<LoadedModule>> modules_;
};

} // namespace Core
