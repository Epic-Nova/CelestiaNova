#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Core/ModuleAPI.h"

namespace Core {

enum class SignalNotificationSeverity {
    Info,
    Success,
    Warning,
    Error,
    Critical,
};

struct SignalNotification {
    std::string Channel;
    std::string SourceExtensionId;
    std::string SourceInstanceId;
    std::string TargetInstanceId;
    std::string TargetMenuId;
    std::string TargetFieldId;
    std::string Code;
    std::string Title;
    std::string Message;
    SignalNotificationSeverity Severity = SignalNotificationSeverity::Info;
    bool Persistent = false;
    std::string CreatedAtUtc;
};

struct SignalNotificationEnvelope {
    std::uint64_t Sequence = 0;
    SignalNotification Notification;
};

// Optional extension surface for event-style notifications that need to be
// consumed by one or more runtime UIs (for example CanvasCore toast feeds).
class NOVA_CORE_API ISignalNotificationBus {
public:
    virtual ~ISignalNotificationBus();

    virtual void PublishSignalNotification(const SignalNotification& notification) = 0;

    virtual std::vector<SignalNotificationEnvelope> ConsumeSignalNotifications(
        std::uint64_t afterSequence,
        std::size_t maxCount,
        std::uint64_t& outLatestSequence) const = 0;

    virtual int GetSignalNotificationBusPriority() const {
        return 0;
    }
};

} // namespace Core
