#pragma once

#include <string>

#include "Core/ModuleAPI.h"

namespace Core {

enum class NovaInstanceConnectivityRole {
    Unknown,
    Standalone,
    Host,
    Client,
};

struct NovaInstanceConnectivitySnapshot {
    std::string ProviderId;
    NovaInstanceConnectivityRole Role = NovaInstanceConnectivityRole::Unknown;
    int ConnectedInstanceCount = 0;
    std::string Summary;
};

// Optional extension surface to report Celestia Nova inter-instance
// connectivity state to runtime UIs.
class NOVA_CORE_API IInstanceConnectivityProvider {
public:
    virtual ~IInstanceConnectivityProvider();

    virtual NovaInstanceConnectivitySnapshot GetInstanceConnectivitySnapshot() const = 0;

    virtual int GetInstanceConnectivityPriority() const {
        return 0;
    }
};

} // namespace Core
