#include "MeshCore.h"
#include "MeshCoreClientDelegate.h"

#include "Core/ExtensionRegistry.h"
#include "Core/FTSTicker.h"
#include "Core/NovaLog.h"
#include "ExtensionSpecific/ICanvasRuntimeSurfaceProvider.h"
#include "ExtensionSpecific/IRemoteControl.h"
#include "IHTTPAgent.h"

#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

namespace {

std::string NowUtcIso8601() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &timestamp);
#else
    utc = *std::gmtime(&timestamp);
#endif
    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

} // namespace

namespace MeshCore {

// The concrete delegate deliberately owns only local queue state. Transport is
// performed by MeshCore through the already hardened HTTPAgent surface; this
// keeps bearer material out of queued jobs and leaves a future messenger
// bridge free to implement IMeshClientDelegate without changing Canvas.
class MeshClientDelegateImpl final : public IMeshClientDelegate {
public:
    bool ConnectToAuthoritativeInstance(const std::string& instanceId) override {
        if (instanceId.empty()) return false;
        std::lock_guard<std::mutex> lock(Mutex_);
        ConnectedInstanceId_ = instanceId;
        return true;
    }

    void DisconnectFromAuthoritativeInstance() override {
        std::lock_guard<std::mutex> lock(Mutex_);
        ConnectedInstanceId_.clear();
    }

    bool IsConnectedToAuthoritativeInstance() const override {
        std::lock_guard<std::mutex> lock(Mutex_);
        return !ConnectedInstanceId_.empty();
    }

    std::string GetConnectedAuthoritativeInstanceId() const override {
        std::lock_guard<std::mutex> lock(Mutex_);
        return ConnectedInstanceId_;
    }

    bool DispatchJob(FMeshWorkJob job) override {
        if (job.JobId.empty() || job.RequestedInstanceId.empty()) return false;
        std::lock_guard<std::mutex> lock(Mutex_);
        if (ConnectedInstanceId_ != job.RequestedInstanceId) return false;
        const auto now = NowUtcIso8601();
        job.State = EMeshJobState::Pending;
        job.CreatedAtUtc = job.CreatedAtUtc.empty() ? now : job.CreatedAtUtc;
        job.UpdatedAtUtc = now;
        Jobs_[job.JobId] = std::move(job);
        return true;
    }

    bool PollJobResults() override {
        std::lock_guard<std::mutex> lock(Mutex_);
        for (auto& entry : Jobs_) {
            if (entry.second.State == EMeshJobState::Pending) {
                entry.second.State = EMeshJobState::Running;
                entry.second.UpdatedAtUtc = NowUtcIso8601();
            }
        }
        return true;
    }

    std::vector<FMeshWorkJob> GetJobSnapshot() const override {
        std::lock_guard<std::mutex> lock(Mutex_);
        std::vector<FMeshWorkJob> snapshot;
        snapshot.reserve(Jobs_.size());
        for (const auto& entry : Jobs_) snapshot.push_back(entry.second);
        return snapshot;
    }

    FMeshClientModeReport BuildReport() const override {
        FMeshClientModeReport report;
        std::lock_guard<std::mutex> lock(Mutex_);
        report.Total = static_cast<int>(Jobs_.size());
        for (const auto& entry : Jobs_) {
            if (entry.second.State == EMeshJobState::Completed) ++report.Completed;
            if (entry.second.State == EMeshJobState::Failed) ++report.Failed;
        }
        return report;
    }

    void CompleteJob(const std::string& jobId, bool succeeded, std::string error) {
        std::lock_guard<std::mutex> lock(Mutex_);
        const auto found = Jobs_.find(jobId);
        if (found == Jobs_.end()) return;
        found->second.State = succeeded ? EMeshJobState::Completed : EMeshJobState::Failed;
        found->second.ErrorMessage = std::move(error);
        found->second.UpdatedAtUtc = NowUtcIso8601();
    }

private:
    mutable std::mutex Mutex_;
    std::string ConnectedInstanceId_;
    std::map<std::string, FMeshWorkJob> Jobs_;
};

} // namespace MeshCore

MeshCoreModule::MeshCoreModule() = default;
MeshCoreModule::~MeshCoreModule() = default;

void MeshCoreModule::StartupModule() {
    ClientDelegate_ = std::make_unique<MeshCore::MeshClientDelegateImpl>();
    TickerHandle_ = Core::FTSTicker::GetCoreTicker().AddTicker([this](float) -> bool {
        return ClientDelegate_ && ClientDelegate_->PollJobResults();
    }, 2.0f);
    NOVA_LOG("[MeshCore] Client delegation queue and polling ticker initialized.", LogType::Log);
}

void MeshCoreModule::ShutdownModule() {
    Core::FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle_);
    if (ClientDelegate_) ClientDelegate_->DisconnectFromAuthoritativeInstance();
    ClientDelegate_.reset();
    std::lock_guard<std::mutex> lock(ReceiptMutex_);
    Receipts_.clear();
    NOVA_LOG("[MeshCore] Client delegation stopped.", LogType::Log);
}

Core::CanvasMenuActionResult MeshCoreModule::OnMenuAction(const Core::CanvasMenuActionRequest& request) {
    Core::CanvasMenuActionResult result;
    if (request.ActionId == "invoke") {
        const auto selected = request.ContextValues.find("selectedExtension");
        if (selected != request.ContextValues.end() && selected->second == "meshcore") {
            result.NavigateToMenuId = "meshcore_main";
            return result;
        }
    }
    if (request.MenuId != "meshcore_main") {
        result.Success = false;
        result.ErrorMessage = "Unhandled MeshCore menu action.";
        return result;
    }
    if (request.ActionId == "meshcore.command.submit") {
        const auto target = request.ContextValues.find("targetId");
        const auto command = request.ContextValues.find("commandId");
        const auto receipt = SubmitRemoteCommand(target == request.ContextValues.end() ? "" : target->second,
                                                 command == request.ContextValues.end() ? "" : command->second);
        result.Success = receipt.accepted;
        result.ConfigUpdates["remoteStatus"] = "Mesh: " + receipt.message;
        if (!receipt.receiptId.empty()) result.ConfigUpdates["receiptId"] = receipt.receiptId;
        if (!receipt.accepted) result.ErrorMessage = receipt.message;
        return result;
    }
    if (request.ActionId == "meshcore.command.status") {
        const auto receiptId = request.ContextValues.find("receiptId");
        if (receiptId == request.ContextValues.end() || receiptId->second.empty()) {
            result.Success = false;
            result.ErrorMessage = "A Mesh command receipt ID is required.";
            return result;
        }
        std::lock_guard<std::mutex> lock(ReceiptMutex_);
        const auto found = Receipts_.find(receiptId->second);
        if (found == Receipts_.end()) {
            result.Success = false;
            result.ErrorMessage = "The requested Mesh command receipt is unavailable.";
            return result;
        }
        result.ConfigUpdates["remoteStatus"] = "Mesh: " + found->second.message;
        result.ConfigUpdates["receiptId"] = found->second.receiptId;
        return result;
    }
    result.Success = false;
    result.ErrorMessage = "Unknown MeshCore action.";
    return result;
}

MeshCore::FRemoteCommandReceipt MeshCoreModule::SubmitRemoteCommand(const std::string& targetId, const std::string& commandId) {
    MeshCore::FRemoteCommandReceipt receipt;
    if (targetId.empty() || commandId.empty()) {
        receipt.message = "A trusted target ID and allowlisted command ID are required.";
        return receipt;
    }
    auto* resolver = dynamic_cast<Core::IRemoteControlTargetResolver*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("contentforge"));
    auto* sessions = dynamic_cast<Core::INovaIdSessionCapabilityProvider*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("laravelorchestrator"));
    auto* transport = dynamic_cast<Core::IHTTPAgent*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("httpagent"));
    if (!resolver || !sessions || !transport || !ClientDelegate_) {
        receipt.message = "Rejected: ContentForge, Nova ID, HTTPAgent, and the Mesh queue are required.";
        return receipt;
    }

    std::string url, method, requiredCapability, error;
    if (!resolver->ResolveAllowlistedRemoteControlRequest(targetId, commandId, url, method, requiredCapability, error)) {
        receipt.message = "Rejected: " + error;
        return receipt;
    }
    Core::RemoteControlDispatchAuthorization authorization;
    if (!sessions->AuthorizeRemoteControlDispatch(targetId, requiredCapability, authorization, error) || authorization.authorizationHeader.empty()) {
        receipt.message = "Rejected: " + error;
        return receipt;
    }

    static std::atomic<unsigned long long> nextReceipt{1};
    receipt.accepted = true;
    receipt.state = MeshCore::ERemoteCommandState::Queued;
    receipt.receiptId = "mesh-" + std::to_string(nextReceipt.fetch_add(1));
    receipt.message = "Authenticated Mesh command queued.";

    MeshCore::FMeshWorkJob job;
    job.JobId = receipt.receiptId;
    job.RequestedInstanceId = targetId;
    job.Payload = "{\"commandId\":\"" + commandId + "\"}";
    ClientDelegate_->ConnectToAuthoritativeInstance(targetId);
    if (!ClientDelegate_->DispatchJob(std::move(job))) {
        receipt.accepted = false;
        receipt.state = MeshCore::ERemoteCommandState::Rejected;
        receipt.message = "Rejected: Mesh delegation queue could not accept the command.";
        return receipt;
    }
    UpdateRemoteReceipt(receipt.receiptId, receipt);

    Core::SecureHttpsRequest request;
    request.url = url;
    request.method = method;
    request.timeoutMs = 10000;
    request.maxResponseBytes = 65536;
    request.headers.emplace("Authorization", std::move(authorization.authorizationHeader));
    request.headers.emplace("Accept", "application/json");
    const auto receiptId = receipt.receiptId;
    const auto dispatchId = transport->DispatchSecureHttpsAsync(request, [this, receiptId](Core::SecureHttpsResponse response) {
        MeshCore::FRemoteCommandReceipt completed;
        completed.receiptId = receiptId;
        completed.accepted = response.transportSucceeded && response.statusCode >= 200 && response.statusCode < 300;
        completed.state = completed.accepted ? MeshCore::ERemoteCommandState::Accepted : MeshCore::ERemoteCommandState::Failed;
        completed.message = completed.accepted ? "Mesh command accepted by target." : "Mesh command failed or was rejected by target.";
        if (ClientDelegate_) ClientDelegate_->CompleteJob(receiptId, completed.accepted, completed.accepted ? "" : completed.message);
        UpdateRemoteReceipt(receiptId, completed);
        PublishToast(completed.accepted ? "MESH_COMMAND_ACCEPTED" : "MESH_COMMAND_FAILED", completed.message,
                     completed.accepted ? Core::CanvasNotificationSeverity::Info : Core::CanvasNotificationSeverity::Warning);
    });
    if (dispatchId.empty()) {
        receipt.accepted = false;
        receipt.state = MeshCore::ERemoteCommandState::Rejected;
        receipt.message = "Rejected: HTTPAgent could not queue the Mesh command.";
        ClientDelegate_->CompleteJob(receipt.receiptId, false, receipt.message);
        UpdateRemoteReceipt(receipt.receiptId, receipt);
    }
    PublishToast(receipt.accepted ? "MESH_COMMAND_QUEUED" : "MESH_COMMAND_REJECTED", receipt.message,
                 receipt.accepted ? Core::CanvasNotificationSeverity::Info : Core::CanvasNotificationSeverity::Warning);
    return receipt;
}

void MeshCoreModule::UpdateRemoteReceipt(const std::string& receiptId, const MeshCore::FRemoteCommandReceipt& receipt) {
    std::lock_guard<std::mutex> lock(ReceiptMutex_);
    Receipts_[receiptId] = receipt;
}

void MeshCoreModule::PublishToast(const std::string& title, const std::string& message, Core::CanvasNotificationSeverity severity) const {
    auto* canvas = dynamic_cast<Core::ICanvasRuntimeSurfaceProvider*>(Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("canvascore"));
    if (!canvas) return;
    Core::CanvasToastNotification toast;
    toast.Id = "meshcore." + title;
    toast.SourceExtensionId = "meshcore";
    toast.TargetMenuId = "meshcore_main";
    toast.Title = title;
    toast.Message = message;
    toast.Severity = severity;
    canvas->PublishCanvasToast(toast);
}

Core::NovaCapabilityDescriptor MeshCoreModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "meshcore";
    descriptor.displayName = "MeshCore";
    descriptor.description = "Federation, delegated jobs, and authenticated remote management for Celestia Nova nodes.";
    descriptor.serviceCapabilities = { "mesh.discovery", "mesh.remote.execute", "mesh.node.list", "mesh.remote.command.submit", "mesh.remote.command.status", "mesh.remote.target.allowlist" };
    descriptor.healthEndpoints = { "/api/v1/health/meshcore" };
    descriptor.contentPacks = { "MeshClientMode" };
    descriptor.telemetryStreams = { "mesh.remote.calls", "mesh.node.count" };
    descriptor.grafanaDashboards = { "grafana://celestianova/meshcore-federation" };
    return descriptor;
}

Core::NovaHealthSnapshot MeshCoreModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = ClientDelegate_ && ClientDelegate_->IsConnectedToAuthoritativeInstance() ? "healthy" : "degraded";
    health.summary = ClientDelegate_ ? "MeshCore delegation queue is ready; connect a trusted node to submit commands." : "MeshCore delegation queue is not initialized.";
    return health;
}

NOVA_DECLARE_MODULE_FACTORY(NOVA_EXPORT, MeshCoreModule)
