#include "ExamplePlugin.h"
#include "Core/NovaLog.h"

#include <sstream>

#if defined(ExamplePlugin_EXPORTS)
#define EXAMPLEPLUGIN_CABI_EXPORT NOVA_EXPORT
#else
#define EXAMPLEPLUGIN_CABI_EXPORT
#endif

ExampleRequirementResolveResult ResolveRequirementForExample(const ExampleRequirementResolveRequest& request) {
    return Core::RequirementResolver::ResolveFromDescriptorDefaults(
        request,
        "exampleplugin",
        "environment.target",
        "environments");
}

extern "C" EXAMPLEPLUGIN_CABI_EXPORT bool ExamplePlugin_ResolveRequirement(const void* requestPtr, void* resultPtr) {
    return Core::RequirementResolver::DispatchResolveRequest(requestPtr, resultPtr, ResolveRequirementForExample);
}

ExampleModule::ExampleModule() {}
ExampleModule::~ExampleModule() {}

void ExampleModule::StartupModule() {
    // Temporarily commenting out cross-DLL struct-by-value API call to trace SIGSEGV
    /*
    const ExampleRequirementResolveRequest sampleRequest{
        "environment.target",
        "canvascore",
        ""
    };
    const ExampleRequirementResolveResult sampleResult = ResolveRequirementForExample(sampleRequest);
    */

    // std::ostringstream message;
    // message << "[ExamplePlugin] StartupModule called - example resolver options: " << sampleResult.Options.size();
    // NOVA_LOG(message.str().c_str(), LogType::Log);

    NOVA_LOG("[ExamplePlugin] StartupModule called", LogType::Log);
}

void ExampleModule::ShutdownModule() {
    NOVA_LOG("[ExamplePlugin] ShutdownModule called", LogType::Log);
}

NOVA_DECLARE_MODULE_FACTORY(EXAMPLEPLUGIN_CABI_EXPORT, ExampleModule)

#undef EXAMPLEPLUGIN_CABI_EXPORT
