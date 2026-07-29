#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace Core {

/**
 * A notification received from another Celestia Nova instance.
 * When IsRemote == true the notification arrived via the messaging service
 * from a connected instance (host, client, or OS service node).
 */
struct FInstanceNotification {
    // The instance that published this notification.
    std::string SourceInstanceId;  // e.g. "host-1", "client-3", "os-service-2"
    std::string SourceRole;        // "host" | "client" | "service"

    // Routing / display fields.
    std::string Channel;           // e.g. "canvas.toast", "canvas.menu.issue", ""
    std::string Code;
    std::string Title;
    std::string Message;
    std::string CreatedAtUtc;

    // True when the notification came through the messaging service bridge
    // from a remote instance rather than being published locally.
    bool IsRemote = false;

    // When Persistent == true the notification is shown in the persistent
    // diagnostics panel rather than (or in addition to) as a toast.
    bool Persistent = false;
};

/**
 * IInstanceNotificationBus
 *
 * Cross-instance notification surface. Extensions (typically MeshCore or
 * SignalCore) implement this to bridge notifications arriving from remote
 * Celestia Nova instances into the local UI layer.
 *
 * Behaviour enforced by CanvasCore:
 *  - Local notifications (IsRemote == false) are displayed as-is.
 *  - Remote notifications (IsRemote == true) have their toast title prefixed
 *    with "[FROM: <SourceInstanceId>]" so the operator can distinguish origin.
 *  - When no menu is open, received notifications are forwarded to NOVA_LOG
 *    instead of the toast queue.
 *
 * Authoritative push rule: only instances holding a valid authoritative JWT
 * (from AegisCore) should push notifications to avoid duplication
 * across the mesh.
 */
class IInstanceNotificationBus {
public:
    virtual ~IInstanceNotificationBus() = default;

    // Publish a notification from this or a remote instance.
    virtual void PublishInstanceNotification(const FInstanceNotification& notification) = 0;

    // Consume up to `max` pending notifications.
    // Call this from the CanvasCore pump loop.
    virtual std::vector<FInstanceNotification> ConsumeNotifications(
        std::uint64_t afterSequence,
        std::size_t max,
        std::uint64_t& outLatestSequence) = 0;
};

} // namespace Core
