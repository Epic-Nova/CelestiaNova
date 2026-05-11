#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "Core/RequirementResolver.h"

#ifdef CoreService_EXPORTS
#  define CORESERVICE_API NOVA_EXPORT
#else
#  define CORESERVICE_API NOVA_IMPORT
#endif

using CoreServiceRequirementResolveRequest = Core::RequirementResolver::CoreRequirementResolveRequest;
using CoreServiceRequirementResolveResult = Core::RequirementResolver::CoreRequirementResolveResult;

CoreServiceRequirementResolveResult ResolveRequirementForCoreService(const CoreServiceRequirementResolveRequest& request);

// CoreService_ResolveRequirement is handled by the ABI export in the .cpp file.

class CORESERVICE_API CoreServiceModule : public IExtensionInterface {
public:
    CoreServiceModule();
    ~CoreServiceModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    void RunStartupSequence();
};

