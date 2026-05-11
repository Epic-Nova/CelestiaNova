#include "ValveCore.h"
#include "Core/NovaLog.h"

ValveCoreModule::ValveCoreModule() {}
ValveCoreModule::~ValveCoreModule() {}

void ValveCoreModule::StartupModule() {
    NOVA_LOG("[ValveCore] StartupModule called. Traffic shaping is active.", LogType::Log);
}

void ValveCoreModule::ShutdownModule() {
    NOVA_LOG("[ValveCore] ShutdownModule called.", LogType::Log);
}

Core::NovaCapabilityDescriptor ValveCoreModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "valvecore";
    descriptor.displayName = "ValveCore";
    descriptor.description = "Traffic shaping and application-level rate limiting core.";
    descriptor.serviceCapabilities = { "valve.limit.check", "valve.policy.list", "valve.stats.reset" };
    descriptor.telemetryStreams = { "valve.request.allowed", "valve.request.denied" };
    return descriptor;
}

Core::NovaHealthSnapshot ValveCoreModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "ValveCore operational. No active floods detected.";
    return health;
}

Core::ValveLimitStatus ValveCoreModule::CheckRequestLimit(const std::string& policyName, 
                                                          const std::string& limitKey, 
                                                          const std::string& contextJson) {
    // TODO: Implement token-bucket or leaky-bucket logic.
    // Integrate with RedisOrchestrator for distributed limit tracking.
    Core::ValveLimitStatus status;
    status.Allowed = true;
    status.PolicyName = policyName;
    status.RemainingRequests = 999; // Placeholder
    return status;
}

std::vector<std::string> ValveCoreModule::ListActivePolicies() const {
    return { "default-api-limit", "auth-attempt-limit" };
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, ValveCoreModule)
