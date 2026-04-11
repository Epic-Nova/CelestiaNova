#include "Core/ModuleManager.h"
#include "Core/ExtensionRegistry.h"
#include "Core/NovaLog.h"
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <chrono>

using namespace Core;

namespace {

enum class VisitState {
    Unvisited,
    Visiting,
    Completed,
};

static std::string JoinCycle(const std::vector<std::string>& cycle) {
    std::string result;
    for (size_t i = 0; i < cycle.size(); ++i) {
        if (i > 0) {
            result += " -> ";
        }
        result += cycle[i];
    }
    return result;
}

static bool DetectCycleFrom(const std::string& node,
                            const std::unordered_map<std::string, std::vector<std::string>>& dependencyMap,
                            std::unordered_map<std::string, VisitState>& state,
                            std::vector<std::string>& activeStack,
                            std::unordered_map<std::string, size_t>& activeIndex,
                            std::vector<std::string>& cycleOut) {
    state[node] = VisitState::Visiting;
    activeIndex[node] = activeStack.size();
    activeStack.push_back(node);

    auto mapIt = dependencyMap.find(node);
    if (mapIt != dependencyMap.end()) {
        for (const auto& dependencyId : mapIt->second) {
            auto stateIt = state.find(dependencyId);
            if (stateIt == state.end()) {
                continue;
            }

            if (stateIt->second == VisitState::Visiting) {
                auto startIt = activeIndex.find(dependencyId);
                if (startIt != activeIndex.end()) {
                    cycleOut.assign(activeStack.begin() + static_cast<long long>(startIt->second), activeStack.end());
                    cycleOut.push_back(dependencyId);
                } else {
                    cycleOut = {node, dependencyId};
                }
                return true;
            }

            if (stateIt->second == VisitState::Unvisited) {
                if (DetectCycleFrom(dependencyId, dependencyMap, state, activeStack, activeIndex, cycleOut)) {
                    return true;
                }
            }
        }
    }

    activeStack.pop_back();
    activeIndex.erase(node);
    state[node] = VisitState::Completed;
    return false;
}

static bool ValidateDependencyGraph(const std::vector<ExtensionDescriptor>& descriptors,
                                    std::string& validationError) {
    validationError.clear();
    if (descriptors.empty()) {
        return true;
    }

    std::unordered_set<std::string> knownIds;
    knownIds.reserve(descriptors.size());

    std::unordered_map<std::string, std::vector<std::string>> dependencyMap;
    dependencyMap.reserve(descriptors.size());

    for (const auto& descriptor : descriptors) {
        if (descriptor.id.empty()) {
            validationError = "Descriptor with empty id cannot participate in dependency graph validation.";
            return false;
        }
        knownIds.insert(descriptor.id);
        dependencyMap[descriptor.id] = descriptor.dependencies;
    }

    for (const auto& descriptor : descriptors) {
        for (const auto& dependencyId : descriptor.dependencies) {
            if (dependencyId.empty()) {
                continue;
            }
            if (dependencyId == descriptor.id) {
                validationError = "Self-dependency detected: " + descriptor.id + " -> " + dependencyId;
                return false;
            }
            if (knownIds.find(dependencyId) == knownIds.end()) {
                validationError = "Missing dependency detected: " + descriptor.id + " -> " + dependencyId;
                return false;
            }
        }
    }

    std::unordered_map<std::string, VisitState> state;
    state.reserve(knownIds.size());
    for (const auto& id : knownIds) {
        state[id] = VisitState::Unvisited;
    }

    std::vector<std::string> activeStack;
    std::unordered_map<std::string, size_t> activeIndex;
    std::vector<std::string> cycle;

    for (const auto& id : knownIds) {
        if (state[id] != VisitState::Unvisited) {
            continue;
        }
        if (DetectCycleFrom(id, dependencyMap, state, activeStack, activeIndex, cycle)) {
            validationError = "Dependency cycle detected: " + JoinCycle(cycle);
            return false;
        }
    }

    return true;
}

static std::vector<std::string> GetTopologicalOrder(const std::vector<ExtensionDescriptor>& descriptors) {
    std::vector<std::string> order;
    std::unordered_map<std::string, std::vector<std::string>> adj;
    std::unordered_map<std::string, int> inDegree;
    std::unordered_set<std::string> allIds;

    for (const auto& d : descriptors) {
        allIds.insert(d.id);
        if (inDegree.find(d.id) == inDegree.end()) inDegree[d.id] = 0;
        for (const auto& dep : d.dependencies) {
            if (dep.empty()) continue;
            adj[dep].push_back(d.id);
            inDegree[d.id]++;
        }
    }

    std::vector<std::string> queue;
    for (const auto& id : allIds) {
        if (inDegree[id] == 0) {
            queue.push_back(id);
        }
    }

    size_t head = 0;
    while (head < queue.size()) {
        std::string u = queue[head++];
        order.push_back(u);
        for (const auto& v : adj[u]) {
            if (--inDegree[v] == 0) {
                queue.push_back(v);
            }
        }
    }

    return order;
}

} // namespace

ModuleManager& ModuleManager::Instance() {
    static ModuleManager inst;
    return inst;
}

int ModuleManager::DiscoverAndLoad(const std::string& pluginsDir) {
    // Delegate discovery to PluginRegistry and then load autostart plugins.
    int registered = ExtensionRegistry::Instance().Discover(pluginsDir);
    if (registered <= 0) return 0;

    auto descs = ExtensionRegistry::Instance().ListExtensionDescriptors();
    std::string dependencyValidationError;
    if (!ValidateDependencyGraph(descs, dependencyValidationError)) {
        NOVA_LOG((std::string("ModuleManager: startup dependency validation failed. ") + dependencyValidationError).c_str(), LogType::Error);
        return 0;
    }

    // Determine topological order to respect dependencies during startup
    std::vector<std::string> loadOrder = GetTopologicalOrder(descs);
    
    // Map IDs to descriptors for quick lookup
    std::unordered_map<std::string, const ExtensionDescriptor*> descMap;
    for (const auto& d : descs) {
        descMap[d.id] = &d;
    }

    int loaded = 0;
    for (const auto& id : loadOrder) {
        auto it = descMap.find(id);
        if (it == descMap.end()) continue;
        
        const auto* d = it->second;
        if (!d->autostart) continue;

        // Apply startup delay sequentially if specified
        if (d->startupDelayMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(d->startupDelayMs));
        }

        if (ExtensionRegistry::Instance().LoadExtensionById(id)) {
            loaded++;
        } else {
            NOVA_LOG((std::string("ModuleManager: failed to autoload plugin '") + id + "'").c_str(), LogType::Error);
        }
    }

    return loaded;
}

void ModuleManager::UnloadAll() {
    ExtensionRegistry::Instance().UnloadAllExtensions();
}
