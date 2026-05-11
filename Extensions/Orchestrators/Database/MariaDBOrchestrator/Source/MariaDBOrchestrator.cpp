#include "MariaDBOrchestrator.h"

#include "Core/NovaLog.h"

MariaDBOrchestratorModule::MariaDBOrchestratorModule() {}
MariaDBOrchestratorModule::~MariaDBOrchestratorModule() {}

#include "Core/ExtensionRegistry.h"
#include "../../../Core/KeyForge/Source/KeyForgeEnvironmentHandoff.h"

void MariaDBOrchestratorModule::StartupModule() {
    NOVA_LOG("[MariaDBOrchestrator] StartupModule called. Requesting secure DB credentials from KeyForge...", LogType::Log);

    IExtensionInterface* keyForgeModule = Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("keyforge");
    if (keyForgeModule) {
        auto* handoff = dynamic_cast<KeyForge::IEnvironmentHandoff*>(keyForgeModule);
        if (handoff) {
            std::string receipt;
            bool accepted = handoff->AcceptEnvironmentTargetHandoff("mariadborchestrator", "db_cluster_primary", receipt);
            if (accepted) {
                NOVA_LOG(("[MariaDBOrchestrator] Successfully received secure credential handoff: " + receipt).c_str(), LogType::Log);
            } else {
                NOVA_LOG("[MariaDBOrchestrator] KeyForge handoff was rejected.", LogType::Warning);
            }
        }
    } else {
        NOVA_LOG("[MariaDBOrchestrator] KeyForge extension not found.", LogType::Warning);
    }
}

void MariaDBOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[MariaDBOrchestrator] ShutdownModule called", LogType::Log);
}
