#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

namespace MeshCore {

/**
 * EMeshJobState – lifecycle of a delegated work job in client mode.
 */
enum class EMeshJobState {
    Pending,    // Queued, not yet acknowledged by the authoritative instance.
    Running,    // Acknowledged and in-flight on the authoritative instance.
    Completed,  // Successfully finished.
    Failed,     // Finished with an error.
};

/**
 * FMeshWorkJob – represents a unit of delegated work dispatched by a
 * client-mode Celestia Nova instance to an authoritative instance.
 */
struct FMeshWorkJob {
    // Stable unique ID for this job (e.g. a UUID generated at dispatch time).
    std::string JobId;
    // The id of the authoritative instance that should execute this job.
    std::string RequestedInstanceId;
    // JSON-serialised request payload.
    std::string Payload;
    // Current lifecycle state.
    EMeshJobState State = EMeshJobState::Pending;
    // ISO-8601 UTC time the job was created.
    std::string CreatedAtUtc;
    // ISO-8601 UTC time the job last changed state.
    std::string UpdatedAtUtc;
    // Human-readable error message when State == Failed.
    std::string ErrorMessage;
};

/**
 * FMeshClientModeReport – summary of queue processing reported back to the UI
 * via IInstanceNotificationBus when all running jobs have settled.
 */
struct FMeshClientModeReport {
    int Completed = 0;
    int Failed    = 0;
    int Total     = 0;
};

/**
 * IMeshClientDelegate
 *
 * Skeleton interface for MeshCore's client-mode delegation subsystem.
 *
 * Client mode flow:
 *  1. ConnectToAuthoritativeInstance(instanceId)
 *     - Establishes a connection (via NexusCore + HTTPAgent) to the
 *       authoritative Celestia Nova instance.
 *     - Fetches status-pill data and connected-instance info from that node.
 *
 *  2. DispatchJob(job)
 *     - Adds the job to the local pending queue.
 *     - Serialises and forwards it to the authoritative instance through the
 *       MessengerOrchestrator queue (e.g. RabbitMQ via RabbitMQOrchestrator).
 *
 *  3. PollJobResults()
 *     - Called periodically by the FTSTicker delegate.
 *     - The authoritative instance fetches responses from the messenger queue
 *       and returns them through the same channel.
 *     - Updates job states in the local queue.
 *
 *  4. When all queued jobs have settled (Completed or Failed):
 *     - Publishes a FMeshClientModeReport via IInstanceNotificationBus so
 *       CanvasCore can display the "N completed / M failed" slide-in toast.
 */
class IMeshClientDelegate {
public:
    virtual ~IMeshClientDelegate() = default;

    // Connect to an authoritative Celestia Nova instance by its mesh id.
    // Returns true on success. The instance must be reachable via NexusCore.
    virtual bool ConnectToAuthoritativeInstance(const std::string& instanceId) = 0;

    // Disconnect from the current authoritative instance.
    virtual void DisconnectFromAuthoritativeInstance() = 0;

    // Returns true if currently connected to an authoritative instance.
    virtual bool IsConnectedToAuthoritativeInstance() const = 0;

    // The id of the currently connected authoritative instance, or "" if none.
    virtual std::string GetConnectedAuthoritativeInstanceId() const = 0;

    // Enqueue a work job for delegation to the authoritative instance.
    // Returns true immediately; the job enters Pending state.
    virtual bool DispatchJob(FMeshWorkJob job) = 0;

    // Tick the polling cycle. Called by the FTSTicker delegate.
    // Checks the messenger queue for responses and updates job states.
    // When all jobs settle, publishes a report via IInstanceNotificationBus.
    // Returns false to unsubscribe from the ticker (used in shutdown).
    virtual bool PollJobResults() = 0;

    // Read-only snapshot of all current jobs (Pending, Running, Completed, Failed).
    virtual std::vector<FMeshWorkJob> GetJobSnapshot() const = 0;

    // Build a summary report from the current job snapshot.
    virtual FMeshClientModeReport BuildReport() const = 0;
};

} // namespace MeshCore
