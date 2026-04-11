#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef AegisCore_EXPORTS
#  define AEGISCORE_API NOVA_EXPORT
#else
#  define AEGISCORE_API NOVA_IMPORT
#endif

class AEGISCORE_API AegisCoreModule : public IExtensionInterface {
public:
    AegisCoreModule();
    ~AegisCoreModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef AegisCore_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(AEGISCORE_API, AegisCoreModule)
#endif

