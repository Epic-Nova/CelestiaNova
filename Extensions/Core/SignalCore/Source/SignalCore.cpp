#include "SignalCore.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "Core/NovaLog.h"

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

SignalCoreModule::SignalCoreModule() {}
SignalCoreModule::~SignalCoreModule() {}

void SignalCoreModule::StartupModule() {
    NOVA_LOG("[SignalCore] StartupModule called", LogType::Log);

    Core::SignalNotification startupSignal;
    startupSignal.Channel = "signal.lifecycle";
    startupSignal.SourceExtensionId = "signalcore";
    startupSignal.Title = "SignalCore online";
    startupSignal.Message = "Signal notification bus is ready.";
    startupSignal.Severity = Core::SignalNotificationSeverity::Info;
    PublishSignalNotification(startupSignal);
}

void SignalCoreModule::ShutdownModule() {
    {
        std::lock_guard<std::mutex> lock(NotificationMutex_);
        Notifications_.clear();
        LastSequence_ = 0;
    }

    NOVA_LOG("[SignalCore] ShutdownModule called", LogType::Log);
}

void SignalCoreModule::PublishSignalNotification(const Core::SignalNotification& notification) {
    Core::SignalNotification sanitized = notification;

    if (sanitized.Channel.empty()) {
        sanitized.Channel = "signal.default";
    }

    if (sanitized.CreatedAtUtc.empty()) {
        sanitized.CreatedAtUtc = NowUtcIso8601();
    }

    std::lock_guard<std::mutex> lock(NotificationMutex_);

    Core::SignalNotificationEnvelope envelope;
    envelope.Sequence = ++LastSequence_;
    envelope.Notification = std::move(sanitized);
    Notifications_.push_back(std::move(envelope));

    while (Notifications_.size() > MaxRetainedNotifications_) {
        Notifications_.pop_front();
    }
}

std::vector<Core::SignalNotificationEnvelope> SignalCoreModule::ConsumeSignalNotifications(
    std::uint64_t afterSequence,
    std::size_t maxCount,
    std::uint64_t& outLatestSequence) const {
    std::vector<Core::SignalNotificationEnvelope> out;

    std::lock_guard<std::mutex> lock(NotificationMutex_);
    outLatestSequence = LastSequence_;

    if (Notifications_.empty()) {
        return out;
    }

    std::size_t effectiveMax = maxCount;
    if (effectiveMax == 0) {
        effectiveMax = Notifications_.size();
    }

    for (const auto& envelope : Notifications_) {
        if (envelope.Sequence <= afterSequence) {
            continue;
        }

        out.push_back(envelope);
        if (out.size() >= effectiveMax) {
            break;
        }
    }

    return out;
}

int SignalCoreModule::GetSignalNotificationBusPriority() const {
    return 100;
}
