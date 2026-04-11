#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "Core/RequirementResolver.h"
#include <string>
#include <vector>

#ifdef ExamplePlugin_EXPORTS
#  define EXAMPLEPLUGIN_API NOVA_EXPORT
#else
#  define EXAMPLEPLUGIN_API NOVA_IMPORT
#endif

using ExampleResolvedOption = Core::RequirementResolver::CoreRequirementResolvedOption;
using ExampleRequirementResolveRequest = Core::RequirementResolver::CoreRequirementResolveRequest;
using ExampleRequirementResolveResult = Core::RequirementResolver::CoreRequirementResolveResult;

ExampleRequirementResolveResult ResolveRequirementForExample(const ExampleRequirementResolveRequest& request);

// Note: ExamplePlugin_ResolveRequirement is exported via extern "C" in
// ExamplePlugin.cpp only. The declaration must not appear in this header
// because the MSVC linker sees it from multiple TUs without the __declspec
// decoration, causing C2375 (redefinition; different linkage).

class ExampleModule : public IExtensionInterface {
public:
    ExampleModule();
    ~ExampleModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

