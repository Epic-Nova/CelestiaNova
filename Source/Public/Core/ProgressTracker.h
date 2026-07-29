#pragma once

#include <string>
#include <vector>
#include "Core/ModuleAPI.h"

namespace Core {

struct NovaProgressSnapshot {
    std::string operationId;
    std::string owner;
    std::string phase;
    int percent = 0;
    bool active = false;
    bool failed = false;
};

// Cross-surface progress contract. Extensions publish here; the CLI, Canvas
// and future Mesh receiver consume the same atomically persisted snapshot.
class NOVA_CORE_API ProgressTracker {
public:
    static void Publish(NovaProgressSnapshot snapshot);
    static NovaProgressSnapshot Read();
    // Redacted phase history for terminal/dashboard activity views.
    static std::vector<std::string> ReadRecentActivity(std::size_t maxLines = 8);
    static std::string DefaultStatusPath();
};

} // namespace Core
