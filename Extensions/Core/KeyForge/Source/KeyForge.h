#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "KeyForgeEnvironmentHandoff.h"

#ifdef KeyForge_EXPORTS
#  define KEYFORGE_API NOVA_EXPORT
#else
#  define KEYFORGE_API NOVA_IMPORT
#endif

class KEYFORGE_API KeyForgeModule : public IExtensionInterface, public KeyForge::IEnvironmentHandoff {
public:
    KeyForgeModule();
    ~KeyForgeModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    bool AcceptEnvironmentTargetHandoff(const std::string& requestorExtensionId,
                                        const std::string& environmentTarget,
                                        std::string& outReceipt) override;
};

#ifdef KeyForge_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(KEYFORGE_API, KeyForgeModule)
#endif

