#pragma once

#include <string>

#include "Core/ModuleAPI.h"

namespace Core {

// Extension-owned daemon status aggregation contract. Transport services use
// this interface rather than rebuilding status data themselves.
// This is a header-only cross-extension contract, not a NovaCore ABI type.
// Decorating it with NOVA_CORE_API makes consuming plug-ins import its inline
// special members from NovaCore on Windows, which breaks their link step.
class IStatusSnapshotProvider {
public:
    virtual ~IStatusSnapshotProvider() = default;

    // Returns a redacted, JSON object payload suitable for a daemon status API.
    virtual std::string BuildDaemonStatusJson() const = 0;
};

} // namespace Core
