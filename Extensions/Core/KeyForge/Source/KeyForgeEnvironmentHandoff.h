#pragma once

#include <string>

namespace KeyForge {

// Optional KeyForge-owned interface for receiving resolved environment targets
// from other extensions (for example, CoreService).
class IEnvironmentHandoff {
public:
    virtual ~IEnvironmentHandoff() = default;

    virtual bool AcceptEnvironmentTargetHandoff(const std::string& requestorExtensionId,
                                                const std::string& environmentTarget,
                                                std::string& outReceipt) = 0;
};

} // namespace KeyForge
