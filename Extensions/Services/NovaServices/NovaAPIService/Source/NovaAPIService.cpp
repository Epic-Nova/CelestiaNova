#include "NovaAPIService.h"

#include "Core/NovaLog.h"
#include "Core/ExtensionRegistry.h"
#include "ExtensionSpecific/IContentForge.h"
#include "../../../Orchestrators/CoreFrameworkOrchestrator/Source/CoreFrameworkOrchestrator.h"

NovaAPIServiceModule::NovaAPIServiceModule() {
    GlobalRateLimiter_ = std::make_unique<Utils::RateLimiter>(10.0, 20.0); // Default 10 req/s
}

NovaAPIServiceModule::~NovaAPIServiceModule() {}

void NovaAPIServiceModule::StartupModule() {
    NOVA_LOG("[NovaAPIService] StartupModule called. API Gateway ready.", LogType::Log);

    auto* contentForge = dynamic_cast<Core::IContentForge*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("contentforge"));
    auto* frameworkOrchestrator = dynamic_cast<CoreFramework::ICoreFrameworkOrchestrator*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("coreframeworkorchestrator"));

    // Application content is declared by ContentForge packs. NovaAPIService
    // must never fetch a hard-coded repository or mount a hard-coded host
    // path during service startup.
    if (contentForge) {
        NOVA_LOG("[NovaAPIService] Content is supplied by declared ContentForge packs.", LogType::Log);
    }

    if (frameworkOrchestrator) {
        CoreFramework::FrameworkConfigPayload payload;
        payload.frameworkName = "laravel";
        payload.contentForgeMountPath = "/var/www/html/nova-api";
        payload.requestedDatabaseType = "mariadb";
        
        auto envVars = frameworkOrchestrator->GenerateBaseEnvironment(payload);
        NOVA_LOG(("[NovaAPIService] Generated Base Environment for " + payload.frameworkName).c_str(), LogType::Log);
        
        std::string entrypoint = frameworkOrchestrator->GetDefaultEntrypoint(payload.frameworkName);
        NOVA_LOG(("[NovaAPIService] Expected Entrypoint: " + entrypoint).c_str(), LogType::Log);
    }
}

void NovaAPIServiceModule::ShutdownModule() {
    NOVA_LOG("[NovaAPIService] ShutdownModule called.", LogType::Log);
}

std::vector<Core::FExtensionCliArgDescriptor> NovaAPIServiceModule::GetCliArgDescriptors() const {
    std::vector<Core::FExtensionCliArgDescriptor> descriptors;
    
    Core::FExtensionCliArgDescriptor rateArg;
    rateArg.Flag = "api-rate-limit";
    rateArg.Description = "Global API rate limit (requests per second).";
    rateArg.RequiresValue = true;
    descriptors.push_back(std::move(rateArg));

    Core::FExtensionCliArgDescriptor burstArg;
    burstArg.Flag = "api-burst-limit";
    burstArg.Description = "Global API burst limit (max concurrent tokens).";
    burstArg.RequiresValue = true;
    descriptors.push_back(std::move(burstArg));

    return descriptors;
}

void NovaAPIServiceModule::ApplyCliArgs(const std::vector<Core::FExtensionCliArg>& args) {
    double rate = 10.0;
    double burst = 20.0;
    bool bUpdated = false;

    for (const auto& arg : args) {
        if (arg.Flag == "api-rate-limit") {
            try {
                rate = std::stod(arg.Value);
                bUpdated = true;
            } catch (...) {
                NOVA_LOG(("[NovaAPIService] Invalid rate limit value: " + arg.Value).c_str(), LogType::Error);
            }
        } else if (arg.Flag == "api-burst-limit") {
            try {
                burst = std::stod(arg.Value);
                bUpdated = true;
            } catch (...) {
                NOVA_LOG(("[NovaAPIService] Invalid burst limit value: " + arg.Value).c_str(), LogType::Error);
            }
        }
    }

    if (bUpdated) {
        NOVA_LOG(("[NovaAPIService] Rate limiting configured via CLI: Rate=" + std::to_string(rate) + ", Burst=" + std::to_string(burst)).c_str(), LogType::Log);
        GlobalRateLimiter_->SetRate(rate, burst);
    }
}
