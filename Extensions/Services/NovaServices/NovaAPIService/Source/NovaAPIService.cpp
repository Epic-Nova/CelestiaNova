#include "NovaAPIService.h"

#include "Core/NovaLog.h"
#include "Core/ExtensionRegistry.h"
#include "Core/ProgressTracker.h"
#include "Core/StatusApiSurface.h"
#include "ExtensionSpecific/IContentForge.h"
#include "../../../Orchestrators/CoreFrameworkOrchestrator/Source/CoreFrameworkOrchestrator.h"
#include <cstdlib>
#include <json.hpp>

#ifndef _WIN32
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

std::string LocalStatusResponse(const std::string& request, int& status) {
    const auto lineEnd = request.find("\r\n");
    const std::string requestLine = request.substr(0, lineEnd);
    if (requestLine.rfind("GET ", 0) != 0) {
        status = 405;
        return R"({"error":"method_not_allowed"})";
    }
    const auto pathEnd = requestLine.find(' ', 4);
    const std::string path = requestLine.substr(4, pathEnd == std::string::npos ? std::string::npos : pathEnd - 4);
    if (path == "/api/v1/health") {
        status = 200;
        return R"({"status":"ok","service":"celestianova-daemon"})";
    }
    if (path == "/api/v1/status") {
        status = 200;
        return Core::StatusApiSurface::BuildExtensionsStatusJson();
    }
    if (path == "/api/v1/progress") {
        const auto progress = Core::ProgressTracker::Read();
        status = 200;
        return nlohmann::json{{"operationId", progress.operationId}, {"owner", progress.owner},
            {"phase", progress.phase}, {"percent", progress.percent}, {"active", progress.active}}.dump();
    }
    status = 404;
    return R"({"error":"not_found"})";
}

} // namespace

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

#ifndef _WIN32
    // This belongs to the long-running daemon, not transient `celest`
    // invocations.  A helper process therefore cannot steal the daemon port.
    const char* enabled = std::getenv("CELESTIA_STATUS_API_ENABLED");
    if (enabled && std::string(enabled) == "1") {
        StatusServerRunning_.store(true);
        StatusServerThread_ = std::thread(&NovaAPIServiceModule::RunLocalStatusServer, this);
    }
#else
    NOVA_LOG("[NovaAPIService] Local status HTTP surface is currently enabled by the Linux service runtime.", LogType::Log);
#endif
}

void NovaAPIServiceModule::ShutdownModule() {
    StatusServerRunning_.store(false);
#ifndef _WIN32
    if (StatusServerSocket_ >= 0) {
        ::shutdown(StatusServerSocket_, SHUT_RDWR);
        ::close(StatusServerSocket_);
        StatusServerSocket_ = -1;
    }
#endif
    if (StatusServerThread_.joinable()) StatusServerThread_.join();
    NOVA_LOG("[NovaAPIService] ShutdownModule called.", LogType::Log);
}

int NovaAPIServiceModule::ResolveStatusPort() const {
    if (const char* configured = std::getenv("CELESTIA_STATUS_PORT"); configured && *configured) {
        try {
            const int port = std::stoi(configured);
            if (port > 0 && port <= 65535) return port;
        } catch (...) {}
    }
    return 9080;
}

void NovaAPIServiceModule::RunLocalStatusServer() {
#ifdef _WIN32
    return;
#else
    const int server = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        NOVA_LOG("[NovaAPIService] Cannot create local status HTTP socket.", LogType::Error);
        return;
    }
    StatusServerSocket_ = server;
    int reuse = 1;
    ::setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(ResolveStatusPort()));
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || ::listen(server, 16) != 0) {
        NOVA_LOG(("[NovaAPIService] Cannot bind local status HTTP surface on 127.0.0.1:" + std::to_string(ResolveStatusPort())).c_str(), LogType::Error);
        ::close(server);
        StatusServerSocket_ = -1;
        return;
    }
    NOVA_LOG(("[NovaAPIService] Local status HTTP surface listening on 127.0.0.1:" + std::to_string(ResolveStatusPort())).c_str(), LogType::Log);
    while (StatusServerRunning_.load()) {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(server, &readable);
        timeval timeout{1, 0};
        const int selected = ::select(server + 1, &readable, nullptr, nullptr, &timeout);
        if (selected <= 0 || !FD_ISSET(server, &readable)) continue;
        const int client = ::accept(server, nullptr, nullptr);
        if (client < 0) continue;
        char buffer[4096]{};
        const ssize_t received = ::recv(client, buffer, sizeof(buffer) - 1, 0);
        int status = 400;
        const std::string body = received > 0 ? LocalStatusResponse(std::string(buffer, static_cast<size_t>(received)), status)
                                               : R"({"error":"bad_request"})";
        const char* reason = status == 200 ? "OK" : status == 404 ? "Not Found" : status == 405 ? "Method Not Allowed" : "Bad Request";
        const std::string response = "HTTP/1.1 " + std::to_string(status) + " " + reason + "\r\n"
            "Content-Type: application/json\r\nConnection: close\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
        ::send(client, response.data(), response.size(), 0);
        ::close(client);
    }
#endif
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
