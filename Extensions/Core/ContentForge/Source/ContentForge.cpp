#include "ContentForge.h"

#include "Core/NovaLog.h"

ContentForgeModule::ContentForgeModule() {}
ContentForgeModule::~ContentForgeModule() {}

void ContentForgeModule::StartupModule() {
    NOVA_LOG("[ContentForge] StartupModule called", LogType::Log);
}

void ContentForgeModule::ShutdownModule() {
    NOVA_LOG("[ContentForge] ShutdownModule called", LogType::Log);
}

Core::NovaCapabilityDescriptor ContentForgeModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "contentforge";
    descriptor.displayName = "ContentForge";
    descriptor.description = "Content pack and extension asset lifecycle core.";
    descriptor.serviceCapabilities = { "content.fetch", "content.install", "content.list" };
    descriptor.healthEndpoints = { "/api/v1/health/contentforge" };
    descriptor.contentPacks = { "ContentPackRuntime" };
    descriptor.telemetryStreams = { "contentforge.fetch.count", "contentforge.pack.count" };
    descriptor.grafanaDashboards = { "grafana://celestianova/contentforge-delivery" };
    return descriptor;
}

Core::NovaHealthSnapshot ContentForgeModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "ContentForge base module initialized";
    return health;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, ContentForgeModule)
