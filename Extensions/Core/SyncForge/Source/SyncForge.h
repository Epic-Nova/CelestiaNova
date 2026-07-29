#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/IExtensionCliProvider.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"
#include <mutex>
#include <string>

#ifdef SyncForge_EXPORTS
#  define SYNCFORGE_API NOVA_EXPORT
#else
#  define SYNCFORGE_API NOVA_IMPORT
#endif

class SYNCFORGE_API SyncForgeModule : public IExtensionInterface,
                                     public Core::IExtensionCliProvider,
                                     public Core::INovaCapabilityProvider {
public:
    SyncForgeModule();
    ~SyncForgeModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    // Secure auto-update routines
    bool PerformSecureUpdateCheck(const std::string& targetVersion);
    std::vector<Core::FExtensionCliArgDescriptor> GetCliArgDescriptors() const override;
    void ApplyCliArgs(const std::vector<Core::FExtensionCliArg>& args) override;
    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;

private:
    mutable std::mutex UpdateMutex_;
    std::string LastUpdateState_ = "not_checked";
    std::string LastUpdateSummary_ = "No update manifest has been checked.";
};

#ifdef SyncForge_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(SYNCFORGE_API, SyncForgeModule)
#endif

