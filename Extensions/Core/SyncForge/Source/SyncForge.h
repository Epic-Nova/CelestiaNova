#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef SyncForge_EXPORTS
#  define SYNCFORGE_API NOVA_EXPORT
#else
#  define SYNCFORGE_API NOVA_IMPORT
#endif

class SYNCFORGE_API SyncForgeModule : public IExtensionInterface {
public:
    SyncForgeModule();
    ~SyncForgeModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef SyncForge_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(SYNCFORGE_API, SyncForgeModule)
#endif

