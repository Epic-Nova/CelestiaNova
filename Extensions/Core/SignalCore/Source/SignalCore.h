#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/ISignalCoreSurfaces.h"

#ifdef SignalCore_EXPORTS
#  define SIGNALCORE_API NOVA_EXPORT
#else
#  define SIGNALCORE_API NOVA_IMPORT
#endif

class SIGNALCORE_API SignalCoreModule : public IExtensionInterface, public Core::ISignalNotificationBus {
public:
    SignalCoreModule();
    ~SignalCoreModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    void PublishSignalNotification(const Core::SignalNotification& notification) override;

    std::vector<Core::SignalNotificationEnvelope> ConsumeSignalNotifications(
        std::uint64_t afterSequence,
        std::size_t maxCount,
        std::uint64_t& outLatestSequence) const override;

    int GetSignalNotificationBusPriority() const override;

private:
    mutable std::mutex NotificationMutex_;
    std::deque<Core::SignalNotificationEnvelope> Notifications_;
    std::uint64_t LastSequence_ = 0;
    std::size_t MaxRetainedNotifications_ = 512;
};

#ifdef SignalCore_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(SIGNALCORE_API, SignalCoreModule)
#endif

