#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "Core/ModuleAPI.h"

namespace Core {

struct ValveLimitStatus {
    bool Allowed = true;
    int64_t RemainingRequests = -1;
    int64_t ResetTimeSeconds = 0;
    std::string PolicyName;
    std::string Reason;
};

// Interface for extensions that provide rate limiting or traffic shaping policies.
// Typically implemented by ValveCore and consumed by Gateway/API services.
class IValvePolicyProvider {
public:
    virtual ~IValvePolicyProvider() = default;

    // Checks if a request is allowed based on the provided key and context.
    // Key is usually an IP address, API key, or User ID.
    virtual ValveLimitStatus CheckRequestLimit(const std::string& policyName, 
                                               const std::string& limitKey, 
                                               const std::string& contextJson) = 0;

    // Reports all active policies and their current consumption metrics.
    virtual std::vector<std::string> ListActivePolicies() const = 0;
};

} // namespace Core
