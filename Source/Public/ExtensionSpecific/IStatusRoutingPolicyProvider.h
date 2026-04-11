#pragma once

#include <string>

#include "Core/ModuleAPI.h"

namespace Core {

enum class StatusDeclarationDomain {
    HealthEndpoints,
    ContentEndpoints,
    GrafanaDashboards,
    ServiceCapabilities,
    ContentPacks,
    TelemetryStreams,
};

// Optional extension-owned policy interface used by generic Core status
// aggregation to route declaration domains to specific provider extensions.
class NOVA_CORE_API IStatusRoutingPolicyProvider {
public:
    virtual ~IStatusRoutingPolicyProvider();

    virtual int GetStatusRoutingPolicyPriority() const {
        return 0;
    }

    virtual bool AcceptsProviderForDomain(StatusDeclarationDomain domain,
                                          const std::string& providerId) const = 0;
};

} // namespace Core
