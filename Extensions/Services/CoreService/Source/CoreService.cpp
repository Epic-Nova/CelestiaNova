#include "CoreService.h"

#include "Core/NovaLog.h"
#include "Core/ExtensionRegistry.h"
#include "KeyForgeEnvironmentHandoff.h"

#include <sstream>

#if defined(CoreService_EXPORTS)
#define CORESERVICE_CABI_EXPORT NOVA_EXPORT
#else
#define CORESERVICE_CABI_EXPORT
#endif

CoreServiceRequirementResolveResult ResolveRequirementForCoreService(const CoreServiceRequirementResolveRequest& request) {
    return Core::RequirementResolver::ResolveFromDescriptorDefaults(
        request,
        "coreservice",
        "service.environment",
        "environments");
}

extern "C" CORESERVICE_CABI_EXPORT bool CoreService_ResolveRequirement(const void* requestPtr, void* resultPtr) {
    return Core::RequirementResolver::DispatchResolveRequest(requestPtr, resultPtr, ResolveRequirementForCoreService);
}

static void RunExampleKeyForgeHandoff(const CoreServiceRequirementResolveResult& resolveResult) {
    std::string selectedEnvironment = "";
    if (!resolveResult.Options.empty()) {
        selectedEnvironment = resolveResult.Options.front().Value;
    }

    if (selectedEnvironment.empty()) {
        NOVA_LOG("[CoreService] Example KeyForge handoff skipped (no resolved environment target)", LogType::Warning);
        return;
    }

    IExtensionInterface* keyForgeModule = Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("keyforge");
    if (!keyForgeModule) {
        NOVA_LOG("[CoreService] Example KeyForge handoff skipped (keyforge not loaded)", LogType::Warning);
        return;
    }

    auto* handoff = dynamic_cast<KeyForge::IEnvironmentHandoff*>(keyForgeModule);
    if (!handoff) {
        NOVA_LOG("[CoreService] Example KeyForge handoff skipped (keyforge does not implement handoff interface)", LogType::Warning);
        return;
    }

    std::string receipt;
    const bool accepted = handoff->AcceptEnvironmentTargetHandoff("coreservice", selectedEnvironment, receipt);

    std::ostringstream message;
    message << "[CoreService] Example KeyForge handoff " << (accepted ? "accepted" : "rejected")
            << " with receipt: " << receipt;
    NOVA_LOG(message.str().c_str(), accepted ? LogType::Log : LogType::Warning);
}

CoreServiceModule::CoreServiceModule() {}
CoreServiceModule::~CoreServiceModule() {}

void CoreServiceModule::StartupModule() {
    const CoreServiceRequirementResolveRequest sampleRequest{
        "service.environment",
        "canvascore",
        ""
    };
    const CoreServiceRequirementResolveResult sampleResult = ResolveRequirementForCoreService(sampleRequest);

    std::ostringstream message;
    message << "[CoreService] Startup resolver options: " << sampleResult.Options.size();
    NOVA_LOG(message.str().c_str(), LogType::Log);

    RunExampleKeyForgeHandoff(sampleResult);

    NOVA_LOG("[CoreService] StartupModule called. Initiating startup sequence...", LogType::Log);
    RunStartupSequence();
}

void CoreServiceModule::RunStartupSequence() {
    NOVA_LOG("[CoreService] Loading ServiceRuntimeProfiles from ContentForge...", LogType::Log);
    NOVA_LOG("[CoreService] Coordinating with CoreFrameworkOrchestrator to setup framework environments.", LogType::Log);
    NOVA_LOG("[CoreService] Instructing CoreWebServerOrchestrator to generate proxy routing.", LogType::Log);
    NOVA_LOG("[CoreService] Startup sequence complete.", LogType::Log);
}

void CoreServiceModule::ShutdownModule() {
    NOVA_LOG("[CoreService] ShutdownModule called", LogType::Log);
}

NOVA_DECLARE_MODULE_FACTORY(CORESERVICE_CABI_EXPORT, CoreServiceModule)

#undef CORESERVICE_CABI_EXPORT
