// PluginRegistry.h — LEGACY COMPATIBILITY SHIM
// All types have been renamed. Include ExtensionRegistry.h directly.
// This file exists only to satisfy translation units that were not yet
// migrated; it will be removed once all callers use ExtensionRegistry.
#pragma once
#include "Core/ExtensionRegistry.h"

namespace Core {
    // Type aliases so any remaining callers of old API names still compile.
    using PluginRegistry = ExtensionRegistry;
}
