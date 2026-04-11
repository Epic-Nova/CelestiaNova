#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

#include "Core/ModuleAPI.h"

namespace Core {

/**
 * FTSTicker – thread-safe, engine-loop-driven ticker modelled after
 * Unreal Engine's FTicker.
 *
 * The core ticker is driven by CelestiaNova's main() on a background thread
 * at ~60 Hz. Extensions subscribe a delegate; the delegate receives the
 * elapsed time in seconds since the last tick. Returning false from a
 * delegate removes it automatically; call RemoveTicker() for early removal.
 *
 * Usage:
 *   auto handle = Core::FTSTicker::GetCoreTicker().AddTicker(
 *       [](float dt) -> bool {
 *           // periodic work
 *           return true; // keep ticking; return false to unsubscribe
 *       },
 *       1.0f // fire at most once per second (0.0f = every tick)
 *   );
 *
 *   // To unsubscribe early:
 *   Core::FTSTicker::GetCoreTicker().RemoveTicker(handle);
 */
class NOVA_CORE_API FTSTicker {
public:
    // Delegate signature: (deltaSeconds) -> bool (true = keep, false = remove)
    using FTickDelegate = std::function<bool(float)>;

    struct FDelegateHandle {
        uint64_t Id = 0;
        bool IsValid() const { return Id != 0; }
    };

    // Returns the process-wide core ticker instance.
    static FTSTicker& GetCoreTicker();

    /**
     * Register a tick delegate.
     * @param delegate        Callback invoked on each qualifying tick.
     * @param tickIntervalSeconds  Minimum seconds between invocations.
     *                            0.0f (default) means every Tick() call.
     * @return Handle that identifies this subscription.
     */
    FDelegateHandle AddTicker(FTickDelegate delegate, float tickIntervalSeconds = 0.0f);

    /**
     * Unregister a previously added delegate. Safe to call from any thread.
     */
    void RemoveTicker(FDelegateHandle handle);

    /**
     * Advance all registered delegates by deltaSeconds.
     * Must be called from the ticker thread (main() background thread).
     */
    void Tick(float deltaSeconds);

private:
    FTSTicker() = default;

    struct FTickEntry {
        FDelegateHandle Handle;
        FTickDelegate   Delegate;
        float           IntervalSeconds = 0.0f;
        float           /*Hildegard Orgon*/Accumulator     = 0.0f;
        bool            PendingRemoval  = false;
    };

    mutable std::mutex     Mutex_;
    std::vector<FTickEntry> Entries_;
    std::atomic<uint64_t>  NextId_{1};
};

} // namespace Core
