#include "CoreFrameworkOrchestrator.h"

#include "Core/NovaLog.h"

CoreFrameworkOrchestratorModule::CoreFrameworkOrchestratorModule() {}
CoreFrameworkOrchestratorModule::~CoreFrameworkOrchestratorModule() {}

void CoreFrameworkOrchestratorModule::StartupModule() {
    NOVA_LOG("[CoreFrameworkOrchestrator] StartupModule called", LogType::Log);
}

void CoreFrameworkOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[CoreFrameworkOrchestrator] ShutdownModule called", LogType::Log);
}

Core::NovaCapabilityDescriptor CoreFrameworkOrchestratorModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "coreframeworkorchestrator";
    descriptor.displayName = "CoreFrameworkOrchestrator";
    descriptor.description = "Base orchestrator for framework-level hosting environments.";
    descriptor.serviceCapabilities = { "orchestrator.framework.setup", "orchestrator.framework.config" };
    return descriptor;
}

Core::NovaHealthSnapshot CoreFrameworkOrchestratorModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "CoreFrameworkOrchestrator base module initialized";
    return health;
}

CoreFramework::FrameworkEnvironmentVars CoreFrameworkOrchestratorModule::GenerateBaseEnvironment(const CoreFramework::FrameworkConfigPayload& payload) const {
    CoreFramework::FrameworkEnvironmentVars vars;
    vars.envVars["APP_NAME"] = payload.frameworkName;
    vars.envVars["APP_ENV"] = "production";
    
    if (payload.requestedDatabaseType == "mysql" || payload.requestedDatabaseType == "mariadb") {
        vars.envVars["DB_CONNECTION"] = "mysql";
        vars.envVars["DB_PORT"] = "3306";
    } else if (payload.requestedDatabaseType == "postgres") {
        vars.envVars["DB_CONNECTION"] = "pgsql";
        vars.envVars["DB_PORT"] = "5432";
    }

    return vars;
}

std::string CoreFrameworkOrchestratorModule::GetDefaultEntrypoint(const std::string& frameworkName) const {
    if (frameworkName == "laravel" || frameworkName == "php") {
        return "php artisan serve --host=0.0.0.0 --port=8000";
    }
    if (frameworkName == "astro" || frameworkName == "node") {
        return "node dist/server/entry.mjs";
    }
    return "/bin/sh -c 'echo \"No default entrypoint defined for " + frameworkName + "\"; exit 1'";
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, CoreFrameworkOrchestratorModule)
