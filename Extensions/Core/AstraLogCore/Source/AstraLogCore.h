#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "Core/NovaLog.h"
#include "ExtensionSpecific/ISignalCoreSurfaces.h"

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

#ifdef AstraLogCore_EXPORTS
#  define ASTRALOGCORE_API NOVA_EXPORT
#else
#  define ASTRALOGCORE_API NOVA_IMPORT
#endif

namespace AstraLog {

/**
 * FLogEntry – a single captured log record from any Celestia Nova instance.
 */
struct FLogEntry {
    // The instance that originally emitted the log.
    std::string SourceInstanceId;
    // Extension or subsystem that generated the message, e.g. "meshcore".
    std::string SourceExtensionId;
    // ISO-8601 UTC timestamp of the message.
    std::string CreatedAtUtc;
    // Severity (mirrors LogType).
    std::string Severity; // "Log" | "Warning" | "Error" | "Fatal"
    // The log message body.
    std::string Message;
    // When true, this entry came from a remote instance via the mesh.
    bool IsRemote = false;
};

} // namespace AstraLog

/**
 * AstraLogCoreModule
 *
 * Skeleton for the distributed audit logging extension.
 *
 * Responsibilities:
 *  - Receive NOVA_LOG entries forwarded from local and remote instances.
 *  - Categorize entries by sender instance, severity, and timestamp.
 *  - (Future) Push log entries to a database via IDatabaseOrchestratorProvider
 *    when one is configured.
 *
 * Authoritative-push rule:
 *  Instances configured as authoritative receive a JWT from NovaID (via
 *  AegisCore). The JWT is stored and optionally retrieved via KeyForge.
 *  Only authoritative instances push log entries to the central store to
 *  prevent duplicate writes across the mesh.
 *
 * TODO: Wire NOVA_LOG macro hooks once NovaLog.h gains the forward callback
 *       hook point (ForwardToAstraLog). For now, logs are received via the
 *       ISignalNotificationBus::ConsumeSignalNotifications pathway.
 */
class ASTRALOGCORE_API AstraLogCoreModule : public IExtensionInterface {
public:
    AstraLogCoreModule();
    ~AstraLogCoreModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    // Ingest a log entry (local or forwarded from a remote instance).
    void IngestLogEntry(const AstraLog::FLogEntry& entry);

    // Retrieve buffered log entries (most recent first) up to `maxEntries`.
    // This is the read surface for the canvas diagnostics panel and future
    // database push worker.
    std::vector<AstraLog::FLogEntry> GetRecentEntries(std::size_t maxEntries = 128) const;

    // Returns true if this instance holds a valid authoritative JWT and is
    // therefore permitted to push log entries to the central store.
    // TODO: implement once AegisCore JWT surface is finalized.
    bool IsAuthoritative() const;

private:
    // Maximum in-memory ring buffer size before oldest entries are dropped.
    static constexpr std::size_t kMaxBufferedEntries = 4096;

    mutable std::mutex Mutex_;
    std::vector<AstraLog::FLogEntry> Buffer_;

    // Whether this node has been configured as authoritative and holds
    // a valid token. Populated in StartupModule.
    bool IsAuthoritative_ = false;
};
