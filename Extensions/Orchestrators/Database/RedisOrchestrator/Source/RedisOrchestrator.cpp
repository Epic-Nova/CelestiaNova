#include "RedisOrchestrator.h"

#include "Core/NovaLog.h"

RedisOrchestratorModule::RedisOrchestratorModule() {}
RedisOrchestratorModule::~RedisOrchestratorModule() {}

void RedisOrchestratorModule::StartupModule() {
    NOVA_LOG("[RedisOrchestrator] StartupModule called", LogType::Log);
}

void RedisOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[RedisOrchestrator] ShutdownModule called", LogType::Log);
}

Core::NovaCapabilityDescriptor RedisOrchestratorModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "redisorchestrator";
    descriptor.displayName = "RedisOrchestrator";
    descriptor.description = "Redis setup and configuration orchestrator.";
    descriptor.serviceCapabilities = { "redis.install", "redis.configure", "redis.health" };
    descriptor.healthEndpoints = { "/api/v1/health/redisorchestrator" };
    descriptor.contentPacks = { "RedisSetup" };
    descriptor.telemetryStreams = { "redisorchestrator.actions" };
    descriptor.grafanaDashboards = { "grafana://celestianova/redis-orchestrator" };
    return descriptor;
}

Core::NovaHealthSnapshot RedisOrchestratorModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "RedisOrchestrator base module initialized";
    return health;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, RedisOrchestratorModule)
