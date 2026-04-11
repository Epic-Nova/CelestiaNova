#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Core/ModuleAPI.h"

namespace Core {

enum class CanvasNotificationSeverity {
    Info,
    Success,
    Warning,
    Error,
    Critical,
};

struct CanvasStatusPillSnapshot {
    std::string ModeLabel;
    int ConnectedInstanceCount = 0;
    std::string ProviderId;
    std::string Summary;
};

struct CanvasToastNotification {
    std::string Id;
    std::string SourceExtensionId;
    std::string SourceInstanceId;
    std::string TargetMenuId;
    std::string TargetFieldId;
    std::string Title;
    std::string Message;
    CanvasNotificationSeverity Severity = CanvasNotificationSeverity::Info;
    std::string CreatedAtUtc;
    int DisplayDurationMs = 4200;
};

struct CanvasPersistentInfoWidget {
    std::string Id;
    std::string MenuId;
    std::string FieldId;
    std::string Code;
    std::string Source;
    std::string Message;
    std::string CreatedAtUtc;
};

// Optional extension surface used by live menu renderers to fetch canvas
// runtime chrome state without hard-linking to CanvasCore internals.
class NOVA_CORE_API ICanvasRuntimeSurfaceProvider {
public:
    virtual ~ICanvasRuntimeSurfaceProvider();

    virtual bool RunCanvasMenuLoop(const std::string& startMenuId,
                                   std::string& outError) = 0;

    virtual CanvasStatusPillSnapshot GetCanvasStatusPill() const = 0;

    virtual std::vector<CanvasToastNotification> ConsumeCanvasToastsForMenu(
        const std::string& menuId,
        std::size_t maxCount) const = 0;

    virtual std::vector<CanvasToastNotification> ConsumeCanvasToasts(std::size_t maxCount) const = 0;

    virtual std::vector<CanvasPersistentInfoWidget> GetCanvasPersistentInfos(const std::string& menuId) const = 0;

    virtual void PublishCanvasToast(const CanvasToastNotification& toast) = 0;
};

} // namespace Core
