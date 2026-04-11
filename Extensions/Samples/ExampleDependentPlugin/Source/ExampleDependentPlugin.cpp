#include "ExampleDependentPlugin.h"
#include "Core/NovaLog.h"
#include "ExamplePluginSharedTypes.h"

namespace {
void ValidateDependencyHeaders() {
    const char* version = ExamplePluginApi::kSharedApiVersion;
    (void)version;
}
}

ExampleDependentModule::ExampleDependentModule() {}
ExampleDependentModule::~ExampleDependentModule() {}

void ExampleDependentModule::StartupModule() {
    ValidateDependencyHeaders();
    NOVA_LOG("[ExampleDependentPlugin] StartupModule called (dependency headers resolved)", LogType::Log);
}

void ExampleDependentModule::ShutdownModule() {
    NOVA_LOG("[ExampleDependentPlugin] ShutdownModule called", LogType::Log);
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, ExampleDependentModule)
