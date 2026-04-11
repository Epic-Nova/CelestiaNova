#include "NovaLoadBalancerService.h"

#include "Core/NovaLog.h"

NovaLoadBalancerServiceModule::NovaLoadBalancerServiceModule() {}
NovaLoadBalancerServiceModule::~NovaLoadBalancerServiceModule() {}

void NovaLoadBalancerServiceModule::StartupModule() {
    NOVA_LOG("[NovaLoadBalancerService] StartupModule called", LogType::Log);
}

void NovaLoadBalancerServiceModule::ShutdownModule() {
    NOVA_LOG("[NovaLoadBalancerService] ShutdownModule called", LogType::Log);
}

Core::NovaCapabilityDescriptor NovaLoadBalancerServiceModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "novaloadbalancerservice";
    descriptor.displayName = "NovaLoadBalancerService";
    descriptor.description = "Service extension for load-balancer lifecycle and balancing policy management.";
    descriptor.serviceCapabilities = { "loadbalancer.backends.list", "loadbalancer.policy.apply", "loadbalancer.health" };
    descriptor.healthEndpoints = { "/api/v1/health/novaloadbalancerservice" };
    descriptor.contentPacks = { "LoadBalancerPolicyPack" };
    descriptor.contentEndpoints = { "/api/v1/content/novaloadbalancerservice", "/api/v1/content/novaloadbalancerservice/policies" };
    descriptor.telemetryStreams = { "novalb.backend.healthy" };
    descriptor.grafanaDashboards = { "grafana://celestianova/loadbalancer-service" };
    return descriptor;
}

Core::NovaHealthSnapshot NovaLoadBalancerServiceModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "NovaLoadBalancerService base module initialized";
    return health;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, NovaLoadBalancerServiceModule)
