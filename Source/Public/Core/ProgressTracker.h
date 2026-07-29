#pragma once

#include <string>
#include "Core/ModuleAPI.h"

namespace Core {

struct NovaProgressSnapshot {
    std::string operationId;
    std::string owner;
    std::string phase;
    int percent = 0;
    bool active = false;
};

// Cross-surface progress contract. Extensions publish here; the CLI, Canvas
// and future Mesh receiver consume the same atomically persisted snapshot.
class NOVA_CORE_API ProgressTracker {
public:
    static void Publish(NovaProgressSnapshot snapshot);
    static NovaProgressSnapshot Read();
    static std::string DefaultStatusPath();
};

} // namespace Core
