#include "Core/ProgressTracker.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <json.hpp>
#include <mutex>
#include <vector>

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

std::filesystem::path ActivityPath() { return StatusPath().parent_path() / "activity.log"; }
}

std::string ProgressTracker::DefaultStatusPath() { return StatusPath().string(); }

void ProgressTracker::Publish(NovaProgressSnapshot snapshot) {
    snapshot.percent = std::clamp(snapshot.percent, 0, 100);
    std::lock_guard<std::mutex> lock(ProgressMutex);
    Current = snapshot;
    try {
        const auto path = StatusPath();
        std::filesystem::create_directories(path.parent_path());
        // Progress is intentionally redacted and is the cross-surface status
        // contract. Keep only this status directory/file readable for a local
        // operator; releases and KeyForge credentials remain private.
        std::error_code permissionError;
        std::filesystem::permissions(path.parent_path(),
            std::filesystem::perms::owner_all |
            std::filesystem::perms::group_read | std::filesystem::perms::group_exec |
            std::filesystem::perms::others_read | std::filesystem::perms::others_exec,
            std::filesystem::perm_options::replace, permissionError);
        const auto temporary = path.string() + ".tmp";
        std::ofstream stream(temporary, std::ios::trunc);
        stream << nlohmann::json{{"operationId", snapshot.operationId}, {"owner", snapshot.owner},
            {"phase", snapshot.phase}, {"percent", snapshot.percent}, {"active", snapshot.active}, {"failed", snapshot.failed}}.dump() << '\n';
        stream.close();
        std::filesystem::rename(temporary, path);
        std::filesystem::permissions(path,
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
            std::filesystem::perms::group_read | std::filesystem::perms::others_read,
            std::filesystem::perm_options::replace, permissionError);

        // This is deliberately only the owner/phase text already exposed by
        // the progress snapshot. It is not a raw process or Docker log.
        const auto activityPath = ActivityPath();
        std::ofstream activity(activityPath, std::ios::app);
        activity << "[" << snapshot.owner << "  " << snapshot.percent << "%] " << snapshot.phase << '\n';
        activity.close();
        std::filesystem::permissions(activityPath,
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
            std::filesystem::perms::group_read | std::filesystem::perms::others_read,
            std::filesystem::perm_options::replace, permissionError);
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

std::vector<std::string> ProgressTracker::ReadRecentActivity(std::size_t maxLines) {
    std::lock_guard<std::mutex> lock(ProgressMutex);
    std::vector<std::string> lines;
    try {
        std::ifstream stream(ActivityPath());
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty()) lines.push_back(std::move(line));
        }
        if (lines.size() > maxLines) {
            lines.erase(lines.begin(), lines.end() - static_cast<std::ptrdiff_t>(maxLines));
        }
    } catch (...) {}
    return lines;
}
} // namespace Core
