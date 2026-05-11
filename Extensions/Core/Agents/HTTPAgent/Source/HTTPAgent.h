#pragma once

#include "IHTTPAgent.h"
#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/IMenuActionProvider.h"
#include "Core/RequirementResolver.h"
#include <mutex>
#include <vector>

namespace Core {

class HTTPAgentModule : 
    public IExtensionInterface, 
    public IHTTPAgent,
    public IMenuActionProvider {
public:
    HTTPAgentModule();
    ~HTTPAgentModule() override;

    // IExtensionInterface
    void StartupModule() override;
    void ShutdownModule() override;

    // IExtensionAgent
    std::string GetAgentId() const override { return "httpagent"; }
    std::string GetAgentName() const override { return "HTTPAgent"; }
    bool IsInstalled() const override;
    bool Install(std::function<void(const std::string&)> onProgress = nullptr) override;
    bool Uninstall() override;
    bool RunCommand(const std::string& command, std::string& outOutput) override;
    bool Configure(const std::string& configKey, const std::string& configValue) override;

    // IHTTPAgent
    std::string Get(const std::string& url, const std::map<std::string, std::string>& headers = {}) override;
    std::string Post(const std::string& url, const std::string& body, const std::map<std::string, std::string>& headers = {}) override;
    bool DownloadFile(const std::string& url, const std::string& destinationPath) override;

    // IMenuActionProvider
    CanvasMenuActionResult OnMenuAction(const CanvasMenuActionRequest& request) override;

    // Requirement Resolver
    Core::RequirementResolver::CoreRequirementResolveResult Resolve(const Core::RequirementResolver::CoreRequirementResolveRequest& request);

private:
    void AddLog(const std::string& message);

    struct LogEntry {
        std::string Timestamp;
        std::string Message;
    };

    mutable std::mutex ProgressMutex_;
    std::vector<LogEntry> Logs_;
};

} // namespace Core

extern "C" {
#ifdef HTTPAgent_EXPORTS
    NOVA_EXPORT bool HTTPAgent_Resolve(const void* requestPtr, void* resultPtr);
#endif
}

#ifdef HTTPAgent_EXPORTS
#  define HTTPAGENT_API NOVA_EXPORT
#else
#  define HTTPAGENT_API NOVA_IMPORT
#endif

NOVA_DECLARE_MODULE_FACTORY(HTTPAGENT_API, Core::HTTPAgentModule)
