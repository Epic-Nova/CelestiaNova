#include "NovaID.h"

#include "Core/NovaLog.h"

NovaIDModule::NovaIDModule() {}
NovaIDModule::~NovaIDModule() {}

void NovaIDModule::StartupModule() {
    NOVA_LOG("[NovaID] StartupModule called", LogType::Log);
}

void NovaIDModule::ShutdownModule() {
    NOVA_LOG("[NovaID] ShutdownModule called", LogType::Log);
}

Core::NovaCapabilityDescriptor NovaIDModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "novaid";
    descriptor.displayName = "NovaID";
    descriptor.description = "Identity bridge extension for OAuth2, 2FA confirmation, and secure token exchange.";
    descriptor.serviceCapabilities = { "auth.oauth2.exchange", "auth.session.validate", "auth.scope.prompt" };
    descriptor.healthEndpoints = { "/api/v1/health/novaid" };
    descriptor.contentPacks = { "NovaIDAuth" };
    descriptor.telemetryStreams = { "novaid.token.exchange", "novaid.2fa.challenge" };
    descriptor.grafanaDashboards = { "grafana://celestianova/novaid-security" };
    return descriptor;
}

Core::NovaHealthSnapshot NovaIDModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "NovaID base module initialized";
    return health;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, NovaIDModule)
