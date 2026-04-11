#pragma once

#include "ModuleAPI.h"

// The module interface is a pure virtual interface. Avoid marking the
// interface class with dllexport/dllimport because inline/defaulted
// methods can cause MSVC to generate import references that complicate
// plugin linking. Factory functions are the exported boundary instead.
class IModuleInterface
{
public:
    virtual ~IModuleInterface() = default;

    // Called when the module is loaded into memory
    virtual void StartupModule() = 0;

    // Called before the module is unloaded from memory
    virtual void ShutdownModule() = 0;
};