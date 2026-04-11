#pragma once

#include <string>
#include <vector>
#include "Core/ModuleAPI.h"

namespace Core {

struct NovaCapabilityDescriptor {
    std::string providerId;
    std::string displayName;
    std::string description;
    std::vector<std::string> serviceCapabilities;
    std::vector<std::string> healthEndpoints;
    std::vector<std::string> contentPacks;
    std::vector<std::string> contentEndpoints;
    std::vector<std::string> telemetryStreams;
    std::vector<std::string> grafanaDashboards;
};

struct NovaHealthSnapshot {
    std::string status = "unknown";      // unknown|healthy|degraded|unhealthy
    std::string summary;
    std::string updatedAtUtc;
};

// Optional extension interface consumed by service-facing API aggregation.
// Extensions that implement this can expose health/capability metadata
// used by status pages and monitoring dashboards.
class NOVA_CORE_API INovaCapabilityProvider {
public:
    virtual ~INovaCapabilityProvider();

    virtual NovaCapabilityDescriptor GetCapabilityDescriptor() const = 0;
    virtual NovaHealthSnapshot GetHealthSnapshot() const = 0;
};

} // namespace Core
