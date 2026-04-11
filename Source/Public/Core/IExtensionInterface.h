#pragma once

#include "ModuleAPI.h"

// IExtensionInterface is the pure virtual interface that all Celestia Nova
// extensions must implement. Avoid marking the interface class itself with
// dllexport/dllimport because inline/defaulted methods can cause MSVC to
// generate import references that complicate extension linking.
// Factory functions (CreateModuleInstance / DestroyModuleInstance) are the
// exported boundary instead — see Core/ModuleAPI.h.
class IExtensionInterface
{
public:
    virtual ~IExtensionInterface() = default;

    // Called when the extension is loaded into memory.
    virtual void StartupModule() = 0;

    // Called before the extension is unloaded from memory.
    virtual void ShutdownModule() = 0;
};
