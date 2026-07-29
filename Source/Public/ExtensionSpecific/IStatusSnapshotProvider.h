#pragma once

#include <string>

#include "Core/ModuleAPI.h"

namespace Core {

// Extension-owned daemon status aggregation contract. Transport services use
// this interface rather than rebuilding status data themselves.
class NOVA_CORE_API IStatusSnapshotProvider {
public:
    virtual ~IStatusSnapshotProvider() = default;

    // Returns a redacted, JSON object payload suitable for a daemon status API.
    virtual std::string BuildDaemonStatusJson() const = 0;
};

} // namespace Core
