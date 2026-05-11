#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/IPackageManagerAgent.h"
#include <string>
#include <functional>

#ifdef PackageManagerAgent_EXPORTS
#  define PACKAGEMANAGERAGENT_API NOVA_EXPORT
#else
#  define PACKAGEMANAGERAGENT_API NOVA_IMPORT
#endif

#include "ExtensionSpecific/IMenuActionProvider.h"
#include "Core/RequirementResolver.h"
#include <mutex>
#include <vector>

namespace Core {

class PACKAGEMANAGERAGENT_API PackageManagerAgentModule : 
    public IExtensionAgent,
    public IPackageManagerAgent,
    public IMenuActionProvider {
public:
    PackageManagerAgentModule();
    ~PackageManagerAgentModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    // IExtensionAgent Implementation
    std::string GetAgentId() const override { return "package-manager-agent"; }
    std::string GetAgentName() const override { return "Package Manager Agent"; }
    bool IsInstalled() const override;
    bool Install(std::function<void(const std::string&)> onProgress = nullptr) override;
    bool Uninstall() override;
    bool RunCommand(const std::string& command, std::string& outOutput) override;
    bool Configure(const std::string& configKey, const std::string& configValue) override;

    // IPackageManagerAgent Implementation
    bool InstallPackage(const std::string& packageName) override;
    bool UninstallPackage(const std::string& packageName) override;
    bool IsPackageInstalled(const std::string& packageName) override;
    bool UpdateAll() override;
    std::string GetUnderlyingManagerName() const override;

    // IMenuActionProvider Implementation
    CanvasMenuActionResult OnMenuAction(const CanvasMenuActionRequest& request) override;

    // Requirement Resolver Implementation (Internal)
    Core::RequirementResolver::CoreRequirementResolveResult Resolve(const Core::RequirementResolver::CoreRequirementResolveRequest& request);

private:
    bool ExecuteCommandInternal(const std::string& command, std::string& outOutput);
    std::string GetPlatformInstallCommand() const;
    std::string GetPlatformUninstallCommand() const;
    std::string GetPlatformCheckCommand() const;

    void AddLog(const std::string& message);
    void SetProgress(float progress, const std::string& step);

    struct LogEntry {
        std::string Timestamp;
        std::string Message;
    };

    mutable std::mutex ProgressMutex_;
    std::vector<LogEntry> Logs_;
    float CurrentProgress_ = 0.0f;
    std::string CurrentStep_ = "IDLE";
};

} // namespace Core

extern "C" PACKAGEMANAGERAGENT_API bool PackageManagerAgent_Resolve(const void* requestPtr, void* resultPtr);

#ifdef PackageManagerAgent_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(PACKAGEMANAGERAGENT_API, Core::PackageManagerAgentModule)
#endif
