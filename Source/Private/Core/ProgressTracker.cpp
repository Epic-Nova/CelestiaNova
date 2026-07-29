#include "Core/ProgressTracker.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <json.hpp>
#include <mutex>

namespace Core {
namespace {
std::mutex ProgressMutex;
NovaProgressSnapshot Current;

std::filesystem::path StatusPath() {
    if (const auto* root = std::getenv("CELESTIA_RUNTIME_ROOT"); root && *root) {
        return std::filesystem::path(root).parent_path() / "status" / "progress.json";
    }
    return std::filesystem::current_path() / "Runtime" / "status" / "progress.json";
}
}

std::string ProgressTracker::DefaultStatusPath() { return StatusPath().string(); }

void ProgressTracker::Publish(NovaProgressSnapshot snapshot) {
    snapshot.percent = std::clamp(snapshot.percent, 0, 100);
    std::lock_guard<std::mutex> lock(ProgressMutex);
    Current = snapshot;
    try {
        const auto path = StatusPath();
        std::filesystem::create_directories(path.parent_path());
        const auto temporary = path.string() + ".tmp";
        std::ofstream stream(temporary, std::ios::trunc);
        stream << nlohmann::json{{"operationId", snapshot.operationId}, {"owner", snapshot.owner},
            {"phase", snapshot.phase}, {"percent", snapshot.percent}, {"active", snapshot.active}, {"failed", snapshot.failed}}.dump() << '\n';
        stream.close();
        std::filesystem::rename(temporary, path);
    } catch (...) {
        // Progress reporting never makes a deployment fail.
    }
}

NovaProgressSnapshot ProgressTracker::Read() {
    std::lock_guard<std::mutex> lock(ProgressMutex);
    try {
        std::ifstream stream(StatusPath());
        nlohmann::json value; stream >> value;
        return {value.value("operationId", ""), value.value("owner", ""), value.value("phase", ""),
            value.value("percent", 0), value.value("active", false), value.value("failed", false)};
    } catch (...) { return Current; }
}
} // namespace Core
