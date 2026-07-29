#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/IExtensionCliProvider.h"
#include "Utils/RateLimiter.h"
#include <atomic>
#include <memory>
#include <thread>

#ifdef NovaAPIService_EXPORTS
#  define NOVAAPISERVICE_API NOVA_EXPORT
#else
#  define NOVAAPISERVICE_API NOVA_IMPORT
#endif

class NOVAAPISERVICE_API NovaAPIServiceModule : 
    public IExtensionInterface,
    public Core::IExtensionCliProvider {
public:
    NovaAPIServiceModule();
    ~NovaAPIServiceModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    // IExtensionCliProvider Implementation
    std::vector<Core::FExtensionCliArgDescriptor> GetCliArgDescriptors() const override;
    void ApplyCliArgs(const std::vector<Core::FExtensionCliArg>& args) override;

private:
    void RunLocalStatusServer();
    int ResolveStatusPort() const;

    std::unique_ptr<Utils::RateLimiter> GlobalRateLimiter_;
    std::atomic<bool> StatusServerRunning_{false};
    std::thread StatusServerThread_;
    int StatusServerSocket_ = -1;
};

#ifdef NovaAPIService_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(NOVAAPISERVICE_API, NovaAPIServiceModule)
#endif
