#include "CoreOrchestrator.h"

#include "Core/NovaLog.h"

CoreOrchestratorModule::CoreOrchestratorModule() {}
CoreOrchestratorModule::~CoreOrchestratorModule() {}

void CoreOrchestratorModule::StartupModule() {
    NOVA_LOG("[CoreOrchestrator] StartupModule called", LogType::Log);
}

void CoreOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[CoreOrchestrator] ShutdownModule called", LogType::Log);
}

Core::NovaCapabilityDescriptor CoreOrchestratorModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "coreorchestrator";
    descriptor.displayName = "CoreOrchestrator";
    descriptor.description = "Base orchestrator contract for setup profile depth, UI surface injection, and lifecycle actions.";
    descriptor.serviceCapabilities = {
        "orchestrator.profiles.list",
        "orchestrator.setup.surface",
        "orchestrator.lifecycle.actions"
    };
    descriptor.healthEndpoints = {"/api/v1/health/coreorchestrator"};
    descriptor.contentPacks = {"OrchestratorProfileTemplates", "InteractionLifecyclePresets"};
    descriptor.contentEndpoints = {
        "/api/v1/content/coreorchestrator/profiles",
        "/api/v1/content/coreorchestrator/lifecycle"
    };
    descriptor.telemetryStreams = {"coreorchestrator.profile.usage"};
    descriptor.grafanaDashboards = {"grafana://celestianova/coreorchestrator-profiles"};
    return descriptor;
}

Core::NovaHealthSnapshot CoreOrchestratorModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "CoreOrchestrator contracts ready";
    return health;
}

std::vector<Core::SetupProfileDepth> CoreOrchestratorModule::GetSupportedProfileDepths() const {
    return {
        Core::SetupProfileDepth::Auto,
        Core::SetupProfileDepth::Simple,
        Core::SetupProfileDepth::Advanced,
        Core::SetupProfileDepth::Expert,
    };
}

Core::OrchestratorSetupSurface CoreOrchestratorModule::BuildSetupSurface(
    const Core::SetupSurfaceRequest& request,
    Core::SetupProfileDepth depth) const {
    Core::OrchestratorSetupSurface surface;
    surface.depth = depth;
    surface.uiContributors = request.activeOrchestrators;
    if (!request.scalingManagerExtensionId.empty()) {
        surface.uiContributors.push_back(request.scalingManagerExtensionId);
    }

    Core::SetupSurfaceSection scalingSection;
    scalingSection.id = "scaling";
    scalingSection.title = "Scaling";
    scalingSection.description = "Controls autoscaling behavior and response thresholds.";
    scalingSection.fields.push_back({
        "enableScaling",
        "Enable scaling for this service",
        Core::SetupFieldType::Toggle,
        false,
        "",
        request.scalingRequested ? "true" : "false",
        {}
    });

    Core::SetupSurfaceSection databaseSection;
    databaseSection.id = "database";
    databaseSection.title = "Database Selection";
    databaseSection.description = "Select a database orchestrator target for secure credential injection.";
    Core::SetupSurfaceField dbField;
    dbField.key = "databaseOrchestrator";
    dbField.label = "Database orchestrator";
    dbField.type = Core::SetupFieldType::Select;
    dbField.required = true;
    for (const auto& orchestrator : request.activeDatabaseOrchestrators) {
        dbField.options.push_back({orchestrator, orchestrator, "Active orchestrator", false});
    }
    databaseSection.fields.push_back(std::move(dbField));

    Core::SetupSurfaceSection environmentSection;
    environmentSection.id = "environment";
    environmentSection.title = "Environment Routing";
    environmentSection.description = "Configure environment targets and rollout policy provided by the scaling manager extension.";
    Core::SetupSurfaceField envField;
    envField.key = "environmentTarget";
    envField.label = "Ship target";
    envField.type = Core::SetupFieldType::Select;
    envField.required = true;
    envField.placeholder = "Provided by scaling manager extension";
    envField.defaultValue = request.environment;
    envField.options = request.managedEnvironmentOptions;
    if (envField.options.empty() && !request.environment.empty()) {
        envField.options.push_back({
            request.environment,
            request.environment,
            "Active environment provided by setup request context",
            true
        });
    }
    environmentSection.fields.push_back(std::move(envField));

    surface.sections.push_back(std::move(scalingSection));
    surface.sections.push_back(std::move(databaseSection));
    surface.sections.push_back(std::move(environmentSection));
    return surface;
}

Core::InteractionLifecycleContract CoreOrchestratorModule::GetInteractionLifecycleContract() const {
    Core::InteractionLifecycleContract contract;
    contract.providerId = "coreorchestrator";
    contract.summary = "Defines configuration, migration, and runtime interaction action contracts for orchestrators.";
    contract.actions = {
        {Core::InteractionLifecycleStage::Configuration, Core::InteractionActionType::Validate, "config.validate", "Validate config", "Validate provided setup and environment settings", false, false},
        {Core::InteractionLifecycleStage::Configuration, Core::InteractionActionType::Apply, "config.apply", "Apply config", "Apply generated service and orchestration settings", true, true},
        {Core::InteractionLifecycleStage::Migration, Core::InteractionActionType::Preview, "migration.preview", "Preview migration", "Preview migrations before apply", false, false},
        {Core::InteractionLifecycleStage::Migration, Core::InteractionActionType::Apply, "migration.apply", "Run migration", "Execute orchestrator-managed migrations", true, true},
        {Core::InteractionLifecycleStage::Interaction, Core::InteractionActionType::Restart, "service.restart", "Restart service", "Restart service after config or migration actions", true, true},
        {Core::InteractionLifecycleStage::Interaction, Core::InteractionActionType::FetchLogs, "service.logs", "Fetch logs", "Fetch orchestrator and service logs", false, false},
    };
    contract.emittedEvents = {
        "orchestrator.config.validated",
        "orchestrator.migration.completed",
        "orchestrator.interaction.executed"
    };
    return contract;
}
