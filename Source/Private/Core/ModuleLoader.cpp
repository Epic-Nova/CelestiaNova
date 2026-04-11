#include "Core/ModuleLoader.h"
#include <iostream>
#include <algorithm>

using namespace Core;

ModuleLoader::~ModuleLoader() {
    // Unload any remaining modules
    for (auto &m : modules_) {
        if (m) UnloadModule(m.get());
    }
}

LoadedModule* ModuleLoader::LoadModule(const std::string& path) {
    std::string err;
    auto lib = std::make_unique<SharedLibrary>();
    if (!lib->Load(path, &err)) {
        std::cerr << "ModuleLoader: failed to load library '" << path << "': " << err << "\n";
        return nullptr;
    }

    auto create_sym = lib->GetSymbol("CreateModuleInstance");
    auto destroy_sym = lib->GetSymbol("DestroyModuleInstance");

    if (!create_sym) {
        std::cerr << "ModuleLoader: CreateModuleInstance not found in " << path << "\n";
        lib->Unload();
        return nullptr;
    }

    using CreateFn = IExtensionInterface* (*)();
    using DestroyFn = void (*)(IExtensionInterface*);

    auto create = reinterpret_cast<CreateFn>(create_sym);
    auto destroy = destroy_sym ? reinterpret_cast<DestroyFn>(destroy_sym) : nullptr;

    IExtensionInterface* inst = nullptr;
    try {
        inst = create();
    } catch (const std::exception& e) {
        std::cerr << "ModuleLoader: CreateModuleInstance threw for " << path << ": " << e.what() << "\n";
        lib->Unload();
        return nullptr;
    }

    if (!inst) {
        std::cerr << "ModuleLoader: CreateModuleInstance returned null in " << path << "\n";
        lib->Unload();
        return nullptr;
    }

    auto lm = std::make_unique<LoadedModule>();
    lm->path = path;
    lm->lib = std::move(lib);
    lm->instance = inst;
    lm->destroyFn = destroy;

    modules_.push_back(std::move(lm));
    return modules_.back().get();
}

void ModuleLoader::UnloadModule(LoadedModule* module) {
    if (!module) return;

    if (module->instance) {
        if (module->destroyFn) {
            module->destroyFn(module->instance);
        } else {
            // last-resort: delete if plugin used default new/delete
            delete module->instance;
        }
        module->instance = nullptr;
    }

    if (module->lib) {
        module->lib->Unload();
        module->lib.reset();
    }

    // remove from internal list
    auto it = std::remove_if(modules_.begin(), modules_.end(), [&](const std::unique_ptr<LoadedModule>& p){ return p.get() == module; });
    modules_.erase(it, modules_.end());
}
