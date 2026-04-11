#include "Core/FTSTicker.h"

#include <algorithm>
#include <vector>

namespace Core {

FTSTicker& FTSTicker::GetCoreTicker() {
    static FTSTicker instance;
    return instance;
}

FTSTicker::FDelegateHandle FTSTicker::AddTicker(FTickDelegate delegate, float tickIntervalSeconds) {
    FDelegateHandle handle;
    handle.Id = NextId_.fetch_add(1, std::memory_order_relaxed);

    FTickEntry entry;
    entry.Handle          = handle;
    entry.Delegate        = std::move(delegate);
    entry.IntervalSeconds = tickIntervalSeconds < 0.0f ? 0.0f : tickIntervalSeconds;
    entry.Accumulator     = 0.0f;
    entry.PendingRemoval  = false;

    std::lock_guard<std::mutex> lock(Mutex_);
    Entries_.push_back(std::move(entry));
    return handle;
}

void FTSTicker::RemoveTicker(FDelegateHandle handle) {
    if (!handle.IsValid()) {
        return;
    }

    std::lock_guard<std::mutex> lock(Mutex_);
    for (auto& entry : Entries_) {
        if (entry.Handle.Id == handle.Id) {
            entry.PendingRemoval = true;
            break;
        }
    }
}

void FTSTicker::Tick(float deltaSeconds) {
    // Snapshot entries under lock to avoid holding the lock during callbacks.
    std::vector<FTickEntry*> toFire;
    {
        std::lock_guard<std::mutex> lock(Mutex_);

        // First pass: advance accumulators and collect delegates to fire.
        for (auto& entry : Entries_) {
            if (entry.PendingRemoval) {
                continue;
            }

            entry.Accumulator += deltaSeconds;
            if (entry.Accumulator >= entry.IntervalSeconds) {
                toFire.push_back(&entry);
            }
        }
    }

    // Fire delegates outside the lock.
    for (auto* entry : toFire) {
        bool keepAlive = false;
        try {
            keepAlive = entry->Delegate(entry->Accumulator);
        } catch (...) {
            // Swallow exceptions from delegate callbacks to keep the ticker alive.
            keepAlive = false;
        }

        std::lock_guard<std::mutex> lock(Mutex_);
        if (!keepAlive) {
            entry->PendingRemoval = true;
        } else {
            // Reset accumulator only if keeping alive (subtract the interval,
            // not reset to zero, to preserve any overshoot).
            entry->Accumulator -= entry->IntervalSeconds;
            if (entry->Accumulator < 0.0f) {
                entry->Accumulator = 0.0f;
            }
        }
    }

    // Compact: remove entries marked for removal.
    {
        std::lock_guard<std::mutex> lock(Mutex_);
        Entries_.erase(
            std::remove_if(Entries_.begin(), Entries_.end(),
                           [](const FTickEntry& e) { return e.PendingRemoval; }),
            Entries_.end());
    }
}

} // namespace Core
