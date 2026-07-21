#include "LaravelOrchestrator.h"

#include "Core/NovaLog.h"
#include "Core/ExtensionRegistry.h"
#include "ExtensionSpecific/IContentForge.h"
#include "ExtensionSpecific/ICanvasRuntimeSurfaceProvider.h"
#include "DockerOrchestrator.h"

#include <filesystem>

namespace {

void PublishDeploymentToast(const std::string& title,
                            const std::string& message,
                            Core::CanvasNotificationSeverity severity) {
    auto* canvas = dynamic_cast<Core::ICanvasRuntimeSurfaceProvider*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("canvascore"));
    if (!canvas) {
        return;
    }

    Core::CanvasToastNotification toast;
    toast.SourceExtensionId = "laravelorchestrator";
    toast.Title = title;
    toast.Message = message;
    toast.Severity = severity;
    canvas->PublishCanvasToast(toast);
}

} // namespace

LaravelOrchestratorModule::LaravelOrchestratorModule() {}
LaravelOrchestratorModule::~LaravelOrchestratorModule() {}

void LaravelOrchestratorModule::StartupModule() {
    NOVA_LOG("[LaravelOrchestrator] StartupModule called", LogType::Log);
}

void LaravelOrchestratorModule::ShutdownModule() {
    NOVA_LOG("[LaravelOrchestrator] ShutdownModule called", LogType::Log);
}

std::vector<Core::FExtensionCliArgDescriptor> LaravelOrchestratorModule::GetCliArgDescriptors() const {
    Core::FExtensionCliArgDescriptor descriptor;
    descriptor.Flag = "deploy-local-content";
    descriptor.Description = "Deploy a registered local Laravel content pack using the minimal profile.";
    descriptor.RequiresValue = true;
    return {descriptor};
}

void LaravelOrchestratorModule::ApplyCliArgs(const std::vector<Core::FExtensionCliArg>& args) {
    for (const auto& argument : args) {
        if (argument.Flag != "deploy-local-content" || argument.Value.empty()) {
            continue;
        }

        const auto deployment = DeployLocalContent(argument.Value);
        const auto message = "[LaravelOrchestrator] " + deployment.message;
        NOVA_LOG(message.c_str(), deployment.succeeded ? LogType::Log : LogType::Error);
    }
}

Core::CanvasMenuActionResult LaravelOrchestratorModule::OnMenuAction(const Core::CanvasMenuActionRequest& request) {
    Core::CanvasMenuActionResult result;
    const bool isLaravelMenu = request.MenuId == "laravel_main" ||
        request.MenuId == "laravelorchestrator::laravel_main";
    if (!isLaravelMenu) {
        result.Success = false;
        return result;
    }

    NOVA_LOG(("[LaravelOrchestrator] Handling menu action: " + request.ActionId).c_str(), LogType::Log);

    auto* contentForge = dynamic_cast<Core::IContentForge*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("contentforge"));
    auto* docker = dynamic_cast<IDockerOrchestrator*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("dockerorchestrator"));
    Core::LocalContentDescriptor content;
    if (!contentForge || !docker || !contentForge->ResolveLocalContent("auth-api", content)) {
        result.Success = false;
        result.ErrorMessage = "The local Auth API content or Docker runtime is unavailable.";
        return result;
    }

    if (request.ActionId == "laravel.auth-api.start") {
        const bool queued = docker->StartComposeAsync(content.path, [](DockerComposeResult deployment) {
            PublishDeploymentToast(
                deployment.succeeded ? "AUTH_API_STARTED" : "AUTH_API_START_FAILED",
                deployment.succeeded ? "The local Auth API is running." : deployment.output,
                deployment.succeeded ? Core::CanvasNotificationSeverity::Success : Core::CanvasNotificationSeverity::Error);
        });
        result.Success = queued;
        result.ErrorMessage = queued ? "" : "The Auth API start action could not be queued.";
        result.ConfigUpdates["authApiStatus"] = queued ? "Runtime Status: Starting" : "Runtime Status: Start failed";
        if (queued) {
            PublishDeploymentToast("AUTH_API_STARTING", "Start action was sent to DockerOrchestrator.", Core::CanvasNotificationSeverity::Info);
        }
        return result;
    }

    if (request.ActionId == "laravel.auth-api.stop") {
        const bool queued = docker->StopComposeAsync(content.path, [](DockerComposeResult deployment) {
            PublishDeploymentToast(
                deployment.succeeded ? "AUTH_API_STOPPED" : "AUTH_API_STOP_FAILED",
                deployment.succeeded ? "The local Auth API Compose services are stopped." : deployment.output,
                deployment.succeeded ? Core::CanvasNotificationSeverity::Success : Core::CanvasNotificationSeverity::Error);
        });
        result.Success = queued;
        result.ErrorMessage = queued ? "" : "The Auth API stop action could not be queued.";
        result.ConfigUpdates["authApiStatus"] = queued ? "Runtime Status: Stopping" : "Runtime Status: Stop failed";
        if (queued) {
            PublishDeploymentToast("AUTH_API_STOPPING", "Stop action was sent to DockerOrchestrator.", Core::CanvasNotificationSeverity::Info);
        }
        return result;
    }

    if (request.ActionId == "laravel.auth-api.refresh") {
        result.ConfigUpdates["authApiStatus"] = docker->IsComposeServiceRunning(content.path, "laravel.test")
            ? "Runtime Status: Running"
            : "Runtime Status: Stopped";
        return result;
    }

    result.Success = false;
    result.ErrorMessage = "Unsupported Laravel menu action.";
    return result;
}

LaravelDeploymentResult LaravelOrchestratorModule::DeployLocalContent(const std::string& contentId, const std::string& profile) const {
    LaravelDeploymentResult deployment;
    if (profile != "minimal") {
        deployment.message = "Only the local minimal profile is available.";
        return deployment;
    }

    auto* contentForge = dynamic_cast<Core::IContentForge*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("contentforge"));
    if (!contentForge) {
        deployment.message = "ContentForge is not loaded.";
        return deployment;
    }

    Core::LocalContentDescriptor content;
    if (!contentForge->ResolveLocalContent(contentId, content)) {
        deployment.message = "The requested local content is not registered.";
        return deployment;
    }

    const std::filesystem::path projectPath(content.path);
    if (!std::filesystem::is_regular_file(projectPath / "artisan") ||
        !std::filesystem::is_regular_file(projectPath / "composer.json") ||
        !std::filesystem::is_regular_file(projectPath / "compose.yaml")) {
        deployment.message = "Local content is not a supported Laravel Compose project.";
        return deployment;
    }

    auto* docker = dynamic_cast<IDockerOrchestrator*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("dockerorchestrator"));
    if (!docker) {
        deployment.message = "DockerOrchestrator is not loaded.";
        return deployment;
    }

    const auto composeResult = docker->StartCompose(content.path);
    deployment.succeeded = composeResult.succeeded;
    deployment.message = composeResult.succeeded ? "Local Laravel content started." : composeResult.output;
    return deployment;
}
