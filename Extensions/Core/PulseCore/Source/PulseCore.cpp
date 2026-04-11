#include "PulseCore.h"

#include "Core/NovaLog.h"

PulseCoreModule::PulseCoreModule() {}
PulseCoreModule::~PulseCoreModule() {}

void PulseCoreModule::StartupModule() {
    NOVA_LOG("[PulseCore] StartupModule called", LogType::Log);
}

void PulseCoreModule::ShutdownModule() {
    NOVA_LOG("[PulseCore] ShutdownModule called", LogType::Log);
}

Core::NovaCapabilityDescriptor PulseCoreModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "pulsecore";
    descriptor.displayName = "PulseCore";
    descriptor.description = "Telemetry ingestion and aggregation core for host and service observability.";
    descriptor.serviceCapabilities = { "telemetry.ingest", "telemetry.query", "telemetry.streams.list" };
    descriptor.healthEndpoints = { "/api/v1/health/pulsecore" };
    descriptor.contentPacks = { "PulseMetrics" };
    descriptor.telemetryStreams = { "pulse.ingest.rate", "pulse.stream.count" };
    descriptor.grafanaDashboards = { "grafana://celestianova/pulsecore-observability" };
    return descriptor;
}

Core::NovaHealthSnapshot PulseCoreModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "PulseCore base module initialized";
    return health;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, PulseCoreModule)
