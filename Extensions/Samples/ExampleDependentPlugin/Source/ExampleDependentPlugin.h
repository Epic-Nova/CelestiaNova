#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef ExampleDependentPlugin_EXPORTS
#  define EXAMPLEDEPENDENTPLUGIN_API NOVA_EXPORT
#else
#  define EXAMPLEDEPENDENTPLUGIN_API NOVA_IMPORT
#endif

class ExampleDependentModule : public IExtensionInterface {
public:
    ExampleDependentModule();
    ~ExampleDependentModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

