#include "NovaGatewayService.h"

#include "Core/NovaLog.h"

NovaGatewayServiceModule::NovaGatewayServiceModule() {}
NovaGatewayServiceModule::~NovaGatewayServiceModule() {}

void NovaGatewayServiceModule::StartupModule() {
    NOVA_LOG("[NovaGatewayService] StartupModule called", LogType::Log);
}

void NovaGatewayServiceModule::ShutdownModule() {
    NOVA_LOG("[NovaGatewayService] ShutdownModule called", LogType::Log);
}

Core::NovaCapabilityDescriptor NovaGatewayServiceModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "novagatewayservice";
    descriptor.displayName = "NovaGatewayService";
    descriptor.description = "Service extension for gateway routing, API edge policy, and ingress management.";
    descriptor.serviceCapabilities = { "gateway.routes.list", "gateway.policy.apply", "gateway.health" };
    descriptor.healthEndpoints = { "/api/v1/health/novagatewayservice" };
    descriptor.contentPacks = { "GatewayPolicyPack" };
    descriptor.contentEndpoints = { "/api/v1/content/novagatewayservice", "/api/v1/content/novagatewayservice/policies" };
    descriptor.telemetryStreams = { "novagateway.route.count" };
    descriptor.grafanaDashboards = { "grafana://celestianova/gateway-service" };
    return descriptor;
}

Core::NovaHealthSnapshot NovaGatewayServiceModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "NovaGatewayService base module initialized";
    return health;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, NovaGatewayServiceModule)
