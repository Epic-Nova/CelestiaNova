#include "NovaFirewallService.h"

#include "Core/NovaLog.h"

NovaFirewallServiceModule::NovaFirewallServiceModule() {}
NovaFirewallServiceModule::~NovaFirewallServiceModule() {}

void NovaFirewallServiceModule::StartupModule() {
    NOVA_LOG("[NovaFirewallService] StartupModule called", LogType::Log);
}

void NovaFirewallServiceModule::ShutdownModule() {
    NOVA_LOG("[NovaFirewallService] ShutdownModule called", LogType::Log);
}

Core::NovaCapabilityDescriptor NovaFirewallServiceModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "novafirewallservice";
    descriptor.displayName = "NovaFirewallService";
    descriptor.description = "Service extension for firewall policy orchestration and host enforcement.";
    descriptor.serviceCapabilities = { "firewall.policy.apply", "firewall.rules.list", "firewall.health" };
    descriptor.healthEndpoints = { "/api/v1/health/novafirewallservice" };
    descriptor.contentPacks = { "FirewallPolicyPack" };
    descriptor.contentEndpoints = { "/api/v1/content/novafirewallservice", "/api/v1/content/novafirewallservice/policies" };
    descriptor.telemetryStreams = { "novafirewall.rule.count" };
    descriptor.grafanaDashboards = { "grafana://celestianova/firewall-service" };
    return descriptor;
}

Core::NovaHealthSnapshot NovaFirewallServiceModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "NovaFirewallService base module initialized";
    return health;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, NovaFirewallServiceModule)
