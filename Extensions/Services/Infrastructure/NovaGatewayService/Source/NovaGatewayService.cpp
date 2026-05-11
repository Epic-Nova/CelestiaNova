#include "NovaGatewayService.h"

#include "Core/NovaLog.h"

NovaGatewayServiceModule::NovaGatewayServiceModule() {
    // Default: 100 requests per minute per identity, with a burst of 150
    RateLimitManager_ = std::make_unique<Utils::RateLimitManager>(100.0 / 60.0, 150.0);
}

NovaGatewayServiceModule::~NovaGatewayServiceModule() {}

void NovaGatewayServiceModule::StartupModule() {
    NOVA_LOG("[NovaGatewayService] StartupModule called. Gateway edge services active.", LogType::Log);
}

void NovaGatewayServiceModule::ShutdownModule() {
    NOVA_LOG("[NovaGatewayService] ShutdownModule called.", LogType::Log);
}

bool NovaGatewayServiceModule::IsRequestAllowed(const std::string& identityKey) {
    bool allowed = RateLimitManager_->Allow(identityKey);
    
    if (!allowed) {
        NOVA_LOG(("[NovaGatewayService] Rate limit exceeded for identity: " + identityKey).c_str(), LogType::Warning);
    }
    
    return allowed;
}

void NovaGatewayServiceModule::SyncRateLimits() {
    // [Scaffolding] Locate the database orchestrator for distributed state
    // auto* db = Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance(DistributedStorageId_);
    // if (db) {
    //     // Publish local rate limit counters to the shared database
    //     // And fetch updates from other gateway instances
    //     NOVA_LOG(("[NovaGatewayService] Syncing rate limits with " + DistributedStorageId_).c_str(), LogType::Log);
    // }
}

Core::NovaCapabilityDescriptor NovaGatewayServiceModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "novagatewayservice";
    descriptor.displayName = "NovaGatewayService";
    descriptor.description = "Service extension for gateway routing, API edge policy, and ingress management.";
    descriptor.serviceCapabilities = { "gateway.routes.list", "gateway.policy.apply", "gateway.health", "gateway.ratelimit.identity" };
    descriptor.healthEndpoints = { "/api/v1/health/novagatewayservice" };
    descriptor.contentPacks = { "GatewayPolicyPack" };
    descriptor.contentEndpoints = { "/api/v1/content/novagatewayservice", "/api/v1/content/novagatewayservice/policies" };
    descriptor.telemetryStreams = { "novagateway.route.count", "novagateway.ratelimit.blocked" };
    descriptor.grafanaDashboards = { "grafana://celestianova/gateway-service" };
    return descriptor;
}

Core::NovaHealthSnapshot NovaGatewayServiceModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "NovaGatewayService edge policy engine initialized with identity-based rate limiting";
    return health;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, NovaGatewayServiceModule)
