#include "AstraLogCore.h"
#include "Core/NovaLog.h"

#include <algorithm>

AstraLogCoreModule::AstraLogCoreModule() {}
AstraLogCoreModule::~AstraLogCoreModule() {}

void AstraLogCoreModule::StartupModule() {
    NOVA_LOG("[AstraLogCore] StartupModule: distributed audit logging initialised.", LogType::Log);

    // TODO: Query AegisCore / KeyForge for an authoritative JWT.
    // If a valid token is found, set IsAuthoritative_ = true and enable
    // the database push worker thread.
    IsAuthoritative_ = false;

    NOVA_LOG("[AstraLogCore] Authoritative push: disabled (no JWT configured).", LogType::Log);
}

void AstraLogCoreModule::ShutdownModule() {
    NOVA_LOG("[AstraLogCore] ShutdownModule: flushing buffered log entries.", LogType::Log);
    // TODO: Flush any pending entries to the database before shutdown.
}

void AstraLogCoreModule::IngestLogEntry(const AstraLog::FLogEntry& entry) {
    std::lock_guard<std::mutex> lock(Mutex_);
    Buffer_.push_back(entry);
    if (Buffer_.size() > kMaxBufferedEntries) {
        Buffer_.erase(Buffer_.begin());
    }
}

std::vector<AstraLog::FLogEntry> AstraLogCoreModule::GetRecentEntries(std::size_t maxEntries) const {
    std::lock_guard<std::mutex> lock(Mutex_);
    const std::size_t count = std::min(maxEntries, Buffer_.size());
    return std::vector<AstraLog::FLogEntry>(Buffer_.end() - static_cast<std::ptrdiff_t>(count), Buffer_.end());
}

bool AstraLogCoreModule::IsAuthoritative() const {
    std::lock_guard<std::mutex> lock(Mutex_);
    return IsAuthoritative_;
}

NOVA_DECLARE_MODULE_FACTORY(ASTRALOGCORE_API, AstraLogCoreModule)
