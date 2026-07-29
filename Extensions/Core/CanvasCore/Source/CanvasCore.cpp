#include "CanvasCore.h"
#include "UnattendedModeManager.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#include <set>

#include "Core/ExtensionDescriptorJson.h"
#include "Core/NovaLog.h"
#include "Core/ExtensionRegistry.h"
#include "Core/RequirementResolver.h"
#include "Core/StatusApiSurface.h"
#include "Core/ProgressTracker.h"
#include "ExtensionSpecific/IInstanceConnectivityProvider.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"
#include "ExtensionSpecific/IOrchestrationSurfaces.h"
#include "ExtensionSpecific/ISignalCoreSurfaces.h"
#include "ExtensionSpecific/IMenuActionProvider.h"
#include "MenuSchema/CanvasMenuRuntime.h"
#include "Utils/CommandLineOptions.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "json.hpp"

#if defined(CanvasCore_EXPORTS)
#define CANVASCORE_CABI_EXPORT NOVA_EXPORT
#else
#define CANVASCORE_CABI_EXPORT
#endif

namespace {

using CanvasCoreResolveRequest = Core::RequirementResolver::CoreRequirementResolveRequest;
using CanvasCoreResolveResult = Core::RequirementResolver::CoreRequirementResolveResult;
using CanvasCoreResolveOption = Core::RequirementResolver::CoreRequirementResolvedOption;

constexpr std::size_t kMaxCanvasToastQueueSize = 64;
constexpr std::size_t kMaxPersistentInfosPerMenu = 64;
constexpr char kGlobalCanvasMenuId[] = "__global__";

enum class EProviderScope {
    Any,
    Orchestrator,
    Service,
};

struct FDescriptorSnapshot {
    Core::ExtensionDescriptor Descriptor;
    std::string DescriptorPath;
    nlohmann::json DescriptorJson;
};

std::string NowUtcIso8601() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t timestamp = std::chrono::system_clock::to_time_t(now);

    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &timestamp);
#else
    utc = *std::gmtime(&timestamp);
#endif

    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

Core::CanvasNotificationSeverity ToCanvasSeverity(const Core::SignalNotificationSeverity severity) {
    switch (severity) {
        case Core::SignalNotificationSeverity::Success:
            return Core::CanvasNotificationSeverity::Success;
        case Core::SignalNotificationSeverity::Warning:
            return Core::CanvasNotificationSeverity::Warning;
        case Core::SignalNotificationSeverity::Error:
            return Core::CanvasNotificationSeverity::Error;
        case Core::SignalNotificationSeverity::Critical:
            return Core::CanvasNotificationSeverity::Critical;
        case Core::SignalNotificationSeverity::Info:
        default:
            return Core::CanvasNotificationSeverity::Info;
    }
}

std::string ToCanvasModeLabel(const Core::NovaInstanceConnectivityRole role) {
    switch (role) {
        case Core::NovaInstanceConnectivityRole::Host:
            return "HOST";
        case Core::NovaInstanceConnectivityRole::Client:
            return "CLIENT";
        case Core::NovaInstanceConnectivityRole::Standalone:
            return "LOCAL";
        case Core::NovaInstanceConnectivityRole::Unknown:
        default:
            return "UNKNOWN";
    }
}

std::string NormalizeMenuId(const std::string& menuId) {
    return menuId.empty() ? std::string(kGlobalCanvasMenuId) : menuId;
}

std::string BuildPersistentInfoDedupKey(const std::string& menuId,
                                        const std::string& fieldId,
                                        const std::string& code,
                                        const std::string& message,
                                        const std::string& source) {
    return NormalizeMenuId(menuId) + "|" + fieldId + "|" + code + "|" + message + "|" + source;
}

bool MenuIdMatches(const std::string& targetMenuId, const std::string& activeMenuId) {
    const std::string normalizedTarget = NormalizeMenuId(targetMenuId);
    const std::string normalizedActive = NormalizeMenuId(activeMenuId);

    if (normalizedTarget == kGlobalCanvasMenuId || normalizedActive == kGlobalCanvasMenuId) {
        return true;
    }

    if (normalizedTarget == normalizedActive) {
        return true;
    }

    const std::string scopedTarget = "::" + normalizedTarget;
    if (normalizedActive.size() > scopedTarget.size() &&
        normalizedActive.compare(normalizedActive.size() - scopedTarget.size(), scopedTarget.size(), scopedTarget) == 0) {
        return true;
    }

    const std::string scopedActive = "::" + normalizedActive;
    if (normalizedTarget.size() > scopedActive.size() &&
        normalizedTarget.compare(normalizedTarget.size() - scopedActive.size(), scopedActive.size(), scopedActive) == 0) {
        return true;
    }

    return false;
}

double ParseDoubleOrDefault(const std::string& value, const double fallback) {
    if (value.empty()) {
        return fallback;
    }

    try {
        return std::stod(value);
    } catch (...) {
        return fallback;
    }
}

float Clamp01(const double value) {
    if (value < 0.0) {
        return 0.0f;
    }
    if (value > 1.0) {
        return 1.0f;
    }
    return static_cast<float>(value);
}

bool GetJsonPathValue(const nlohmann::json& root, const std::string& path, nlohmann::json& outValue) {
    if (path.empty()) return false;
    
    std::vector<std::string> segments;
    std::string current;
    for (char ch : path) {
        if (ch == '.') {
            if (!current.empty()) segments.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) segments.push_back(current);

    const nlohmann::json* node = &root;
    for (const auto& segment : segments) {
        if (!node->is_object() || !node->contains(segment)) return false;
        node = &((*node)[segment]);
    }
    outValue = *node;
    return true;
}

bool SetJsonPathValue(nlohmann::json& root, const std::string& path, const nlohmann::json& value) {
    if (path.empty()) return false;
    
    std::vector<std::string> segments;
    std::string current;
    for (char ch : path) {
        if (ch == '.') {
            if (!current.empty()) segments.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) segments.push_back(current);

    if (segments.empty()) return false;

    nlohmann::json* node = &root;
    for (std::size_t idx = 0; idx < segments.size(); ++idx) {
        const std::string& segment = segments[idx];
        const bool isLeaf = idx + 1 == segments.size();
        if (isLeaf) {
            (*node)[segment] = value;
            return true;
        }
        nlohmann::json& next = (*node)[segment];
        if (!next.is_object()) next = nlohmann::json::object();
        node = &next;
    }
    return false;
}

nlohmann::json LoadGlobalConfig() {
    try {
        namespace fs = std::filesystem;
        const fs::path configPath = fs::current_path() / "Configs" / "app_config.json";
        if (fs::exists(configPath)) {
            std::ifstream configFile(configPath);
            nlohmann::json configJson;
            configFile >> configJson;
            return configJson;
        }
    } catch (...) {}
    return nlohmann::json::object();
}

void SaveGlobalConfig(const nlohmann::json& config) {
    try {
        namespace fs = std::filesystem;
        const fs::path configDir = fs::current_path() / "Configs";
        if (!fs::exists(configDir)) {
            fs::create_directories(configDir);
        }
        const fs::path configPath = configDir / "app_config.json";
        std::ofstream configFile(configPath);
        configFile << config.dump(4);
    } catch (...) {}
}

std::string FieldTypeLabel(const CanvasCore::MenuSchema::ECanvasFieldType fieldType) {
    using CanvasCore::MenuSchema::ECanvasFieldType;
    switch (fieldType) {
        case ECanvasFieldType::Bool:
            return "Bool";
        case ECanvasFieldType::String:
            return "String";
        case ECanvasFieldType::PasswordString:
            return "PasswordString";
        case ECanvasFieldType::Integer:
            return "Integer";
        case ECanvasFieldType::Float:
            return "Float";
        case ECanvasFieldType::ProviderSelect:
            return "ProviderSelect";
        case ECanvasFieldType::ProviderMultiSelect:
            return "ProviderMultiSelect";
        case ECanvasFieldType::ToggleGroup:
            return "ToggleGroup";
        case ECanvasFieldType::RadioGroup:
            return "RadioGroup";
        case ECanvasFieldType::Dropdown:
            return "Dropdown";
        case ECanvasFieldType::SliderInt:
            return "SliderInt";
        case ECanvasFieldType::SliderFloat:
            return "SliderFloat";
        case ECanvasFieldType::ActionButton:
            return "ActionButton";
        case ECanvasFieldType::TextLabel:
            return "TextLabel";
        case ECanvasFieldType::Paragraph:
            return "Paragraph";
        case ECanvasFieldType::Separator:
            return "Separator";
        case ECanvasFieldType::Spacer:
            return "Spacer";
        case ECanvasFieldType::Table:
            return "Table";
        case ECanvasFieldType::ProgressGauge:
            return "ProgressGauge";
        case ECanvasFieldType::DirectionalGauge:
            return "DirectionalGauge";
        case ECanvasFieldType::SparklineGraph:
            return "SparklineGraph";
        case ECanvasFieldType::Spinner:
            return "Spinner";
        case ECanvasFieldType::CanvasChart:
            return "CanvasChart";
        case ECanvasFieldType::StructObject:
            return "StructObject";
        case ECanvasFieldType::WindowPane:
            return "WindowPane";
        case ECanvasFieldType::CollapsibleSection:
            return "CollapsibleSection";
        case ECanvasFieldType::ResizableSplit:
            return "ResizableSplit";
        case ECanvasFieldType::TabContainer:
            return "TabContainer";
        default:
            return "Unknown";
    }
}

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string NormalizePath(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    return ToLower(std::move(value));
}

bool IsOrchestratorDescriptorPath(const std::string& descriptorPath) {
    const std::string normalized = NormalizePath(descriptorPath);
    return normalized.find("/extensions/orchestrators/") != std::string::npos ||
           normalized.find("extensions/orchestrators/") != std::string::npos;
}

bool IsServiceDescriptorPath(const std::string& descriptorPath) {
    const std::string normalized = NormalizePath(descriptorPath);
    return normalized.find("/extensions/services/") != std::string::npos ||
           normalized.find("extensions/services/") != std::string::npos;
}

const nlohmann::json* TryReadPath(const nlohmann::json& object,
                                  const std::vector<std::string>& pathSegments) {
    const nlohmann::json* current = &object;
    for (const auto& segment : pathSegments) {
        if (!current->is_object()) {
            return nullptr;
        }

        const auto it = current->find(segment);
        if (it == current->end()) {
            return nullptr;
        }

        current = &(*it);
    }

    return current;
}

std::vector<std::string> ReadStringArrayAtPath(const nlohmann::json& object,
                                               const std::vector<std::string>& pathSegments) {
    std::vector<std::string> values;
    const auto* node = TryReadPath(object, pathSegments);
    if (!node || !node->is_array()) {
        return values;
    }

    for (const auto& entry : *node) {
        if (!entry.is_string()) {
            continue;
        }
        const std::string value = entry.get<std::string>();
        if (!value.empty()) {
            values.push_back(value);
        }
    }

    return values;
}

std::string ReadContextValue(const CanvasCoreResolveRequest& request, const std::string& key) {
    for (const auto& entry : request.ContextValues) {
        if (entry.first == key) {
            return entry.second;
        }
    }

    return "";
}

bool IsExcludedProvider(const std::string& extensionId, EProviderScope scope) {
    if (scope == EProviderScope::Orchestrator) {
        return extensionId == "coreorchestrator";
    }
    if (scope == EProviderScope::Service) {
        return extensionId == "coreservice";
    }

    return false;
}

bool MatchesScope(const FDescriptorSnapshot& descriptor, EProviderScope scope) {
    if (scope == EProviderScope::Any) {
        return true;
    }

    if (scope == EProviderScope::Orchestrator) {
        return IsOrchestratorDescriptorPath(descriptor.DescriptorPath);
    }

    if (scope == EProviderScope::Service) {
        return IsServiceDescriptorPath(descriptor.DescriptorPath);
    }

    return false;
}

std::vector<FDescriptorSnapshot> CollectDescriptorSnapshots() {
    auto& registry = Core::ExtensionRegistry::Instance();
    registry.Discover("Extensions");

    std::vector<FDescriptorSnapshot> snapshots;
    for (const auto& descriptor : registry.ListExtensionDescriptors()) {
        FDescriptorSnapshot snapshot;
        snapshot.Descriptor = descriptor;
        snapshot.DescriptorPath = registry.GetExtensionDescriptorPath(descriptor.id);
        snapshot.DescriptorJson = Core::ExtensionDescriptorJson::LoadDescriptorJsonById(descriptor.id);
        snapshots.push_back(std::move(snapshot));
    }

    std::sort(snapshots.begin(), snapshots.end(), [](const FDescriptorSnapshot& left, const FDescriptorSnapshot& right) {
        const std::string leftName = left.Descriptor.name.empty() ? left.Descriptor.id : left.Descriptor.name;
        const std::string rightName = right.Descriptor.name.empty() ? right.Descriptor.id : right.Descriptor.name;
        if (leftName == rightName) {
            return left.Descriptor.id < right.Descriptor.id;
        }

        return leftName < rightName;
    });

    return snapshots;
}

std::string ResolveDisplayName(const FDescriptorSnapshot& descriptor) {
    if (descriptor.DescriptorJson.is_object()) {
        const auto nameIt = descriptor.DescriptorJson.find("name");
        if (nameIt != descriptor.DescriptorJson.end() && nameIt->is_string()) {
            const std::string value = nameIt->get<std::string>();
            if (!value.empty()) {
                return value;
            }
        }
    }

    if (!descriptor.Descriptor.name.empty()) {
        return descriptor.Descriptor.name;
    }

    return descriptor.Descriptor.id;
}

std::string ExtractDescriptorFolderTag(const FDescriptorSnapshot& descriptor) {
    std::string path = descriptor.DescriptorPath;
    if (path.empty()) {
        return "";
    }

    std::replace(path.begin(), path.end(), '\\', '/');
    std::string tag;

    std::size_t pos = path.find("/Extensions/");
    if (pos == std::string::npos) {
        pos = path.find("/extensions/");
    }

    if (pos != std::string::npos) {
        tag = path.substr(pos + std::string("/Extensions/").size());
    } else {
        tag = path;
    }

    std::size_t lastSlash = tag.find_last_of('/');
    if (lastSlash != std::string::npos) {
        tag = tag.substr(0, lastSlash);
    }

    return tag;
}

void AddUniqueOption(std::vector<CanvasCoreResolveOption>& outOptions,
                     std::set<std::string>& seenValues,
                     const std::string& label,
                     const std::string& value,
                     const std::string& description) {
    if (value.empty()) {
        return;
    }

    if (!seenValues.insert(value).second) {
        return;
    }

    CanvasCoreResolveOption option;
    option.Label = label.empty() ? value : label;
    option.Value = value;
    option.Description = description;
    outOptions.push_back(std::move(option));
}

std::vector<CanvasCoreResolveOption> BuildInstalledProviderOptions(EProviderScope scope) {
    struct FProviderRow {
        std::string FolderTag;
        std::string DisplayName;
        std::string Id;
        std::string Description;
    };

    std::vector<FProviderRow> rows;
    for (const auto& descriptor : CollectDescriptorSnapshots()) {
        if (!MatchesScope(descriptor, scope)) {
            continue;
        }
        if (IsExcludedProvider(descriptor.Descriptor.id, scope)) {
            continue;
        }

        FProviderRow row;
        row.FolderTag = ExtractDescriptorFolderTag(descriptor);
        row.DisplayName = ResolveDisplayName(descriptor);
        row.Id = descriptor.Descriptor.id;
        row.Description = descriptor.Descriptor.description;
        rows.push_back(std::move(row));
    }

    std::sort(rows.begin(), rows.end(), [](const FProviderRow& left, const FProviderRow& right) {
        if (left.FolderTag == right.FolderTag) {
            if (left.DisplayName == right.DisplayName) {
                return left.Id < right.Id;
            }
            return left.DisplayName < right.DisplayName;
        }
        return left.FolderTag < right.FolderTag;
    });

    std::vector<CanvasCoreResolveOption> options;
    std::set<std::string> seen;
    for (const auto& row : rows) {
        std::string label = row.DisplayName;
        if (!row.FolderTag.empty()) {
            label = "[" + row.FolderTag + "] " + row.DisplayName;
        }
        AddUniqueOption(options, seen, label, row.Id, row.Description);
    }

    return options;
}

std::vector<std::string> CollectProviderContentPacks(const FDescriptorSnapshot& descriptor) {
    std::set<std::string> packs;

    const auto optionalPacks = ReadStringArrayAtPath(descriptor.DescriptorJson, {"optional", "contentPacks"});
    for (const auto& pack : optionalPacks) {
        packs.insert(pack);
    }

    auto* instance = Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance(descriptor.Descriptor.id);
    if (instance) {
        auto* capabilityProvider = dynamic_cast<Core::INovaCapabilityProvider*>(instance);
        if (capabilityProvider) {
            try {
                const auto capability = capabilityProvider->GetCapabilityDescriptor();
                for (const auto& pack : capability.contentPacks) {
                    if (!pack.empty()) {
                        packs.insert(pack);
                    }
                }
            } catch (...) {
            }
        }
    }

    return std::vector<std::string>(packs.begin(), packs.end());
}

std::vector<std::string> CollectProviderServiceCapabilities(const FDescriptorSnapshot& descriptor) {
    std::set<std::string> capabilities;

    const auto optionalCapabilities = ReadStringArrayAtPath(descriptor.DescriptorJson, {"optional", "serviceCapabilities"});
    for (const auto& capability : optionalCapabilities) {
        capabilities.insert(capability);
    }

    auto* instance = Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance(descriptor.Descriptor.id);
    if (instance) {
        auto* capabilityProvider = dynamic_cast<Core::INovaCapabilityProvider*>(instance);
        if (capabilityProvider) {
            try {
                const auto capability = capabilityProvider->GetCapabilityDescriptor();
                for (const auto& capabilityId : capability.serviceCapabilities) {
                    if (!capabilityId.empty()) {
                        capabilities.insert(capabilityId);
                    }
                }
            } catch (...) {
            }
        }
    }

    return std::vector<std::string>(capabilities.begin(), capabilities.end());
}

std::vector<CanvasCoreResolveOption> BuildContentPackOptions(const std::string& providerId) {
    std::vector<CanvasCoreResolveOption> options;
    std::set<std::string> seen;
    if (providerId.empty()) {
        return options;
    }

    const auto snapshots = CollectDescriptorSnapshots();
    auto match = std::find_if(snapshots.begin(), snapshots.end(), [&providerId](const FDescriptorSnapshot& descriptor) {
        return descriptor.Descriptor.id == providerId;
    });

    if (match == snapshots.end()) {
        return options;
    }

    for (const auto& pack : CollectProviderContentPacks(*match)) {
        AddUniqueOption(options, seen, pack, pack, "Installed content pack");
    }

    return options;
}

std::vector<CanvasCoreResolveOption> BuildOrchestratorInteractionOptions(const std::string& providerId,
                                                                         const std::string& contentPackId) {
    std::vector<CanvasCoreResolveOption> options;
    std::set<std::string> seen;

    if (providerId.empty()) {
        return options;
    }

    auto addFromLifecycleProvider = [&](const std::string& sourceProviderId, const std::string& sourceTag) {
        auto* instance = Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance(sourceProviderId);
        if (!instance) {
            return;
        }

        auto* interactionProvider = dynamic_cast<Core::IOrchestratorInteractionLifecycleProvider*>(instance);
        if (!interactionProvider) {
            return;
        }

        try {
            const auto contract = interactionProvider->GetInteractionLifecycleContract();
            for (const auto& action : contract.actions) {
                const std::string actionId = action.actionId;
                const std::string label = action.displayName.empty() ? actionId : action.displayName;
                std::string description = action.description;
                if (!contentPackId.empty()) {
                    description = "Pack '" + contentPackId + "': " + description;
                }
                if (!sourceTag.empty()) {
                    if (!description.empty()) {
                        description += " ";
                    }
                    description += "(" + sourceTag + ")";
                }
                AddUniqueOption(options, seen, label, actionId, description);
            }
        } catch (...) {
        }
    };

    addFromLifecycleProvider(providerId, "provider");
    if (options.empty() && providerId != "coreorchestrator") {
        addFromLifecycleProvider("coreorchestrator", "coreorchestrator fallback");
    }

    return options;
}

std::vector<CanvasCoreResolveOption> BuildServiceInteractionOptions(const std::string& providerId,
                                                                    const std::string& contentPackId) {
    std::vector<CanvasCoreResolveOption> options;
    std::set<std::string> seen;

    if (providerId.empty()) {
        return options;
    }

    const auto snapshots = CollectDescriptorSnapshots();
    auto match = std::find_if(snapshots.begin(), snapshots.end(), [&providerId](const FDescriptorSnapshot& descriptor) {
        return descriptor.Descriptor.id == providerId;
    });

    auto addFromSnapshotCapabilities = [&](const FDescriptorSnapshot& descriptor, const std::string& sourceTag) {
        for (const auto& capabilityId : CollectProviderServiceCapabilities(descriptor)) {
            std::string description = "Service interaction capability";
            if (!contentPackId.empty()) {
                description = "Pack '" + contentPackId + "': " + description;
            }
            if (!sourceTag.empty()) {
                description += " (" + sourceTag + ")";
            }
            AddUniqueOption(options, seen, capabilityId, capabilityId, description);
        }
    };

    if (match != snapshots.end()) {
        addFromSnapshotCapabilities(*match, "provider");
    }

    if (options.empty()) {
        auto coreService = std::find_if(snapshots.begin(), snapshots.end(), [](const FDescriptorSnapshot& descriptor) {
            return descriptor.Descriptor.id == "coreservice";
        });
        if (coreService != snapshots.end()) {
            addFromSnapshotCapabilities(*coreService, "coreservice fallback");
        }
    }

    return options;
}

std::vector<CanvasCoreResolveOption> BuildHelpCatalogEndpointOptions() {
    std::vector<CanvasCoreResolveOption> options;
    std::set<std::string> seen;

    AddUniqueOption(options,
                    seen,
                    "Installed Extensions Status",
                    "/api/v1/status/extensions",
                    "Aggregated extension inventory and health surface");

    for (const auto& endpoint : Core::StatusApiSurface::ListDeclaredHealthEndpoints()) {
        AddUniqueOption(options, seen, endpoint, endpoint, "Declared health endpoint");
    }

    for (const auto& endpoint : Core::StatusApiSurface::ListDeclaredContentEndpoints()) {
        AddUniqueOption(options, seen, endpoint, endpoint, "Declared content endpoint");
    }

    return options;
}

std::vector<CanvasCoreResolveOption> BuildHelpTopicOptions(const std::string& providerId) {
    std::vector<CanvasCoreResolveOption> options;
    std::set<std::string> seen;

    if (providerId.empty()) {
        return options;
    }

    const auto snapshots = CollectDescriptorSnapshots();
    auto match = std::find_if(snapshots.begin(), snapshots.end(), [&providerId](const FDescriptorSnapshot& descriptor) {
        return descriptor.Descriptor.id == providerId;
    });

    if (match == snapshots.end()) {
        return options;
    }

    for (const auto& capability : CollectProviderServiceCapabilities(*match)) {
        AddUniqueOption(options,
                        seen,
                        "Capability: " + capability,
                        "capability:" + capability,
                        "Extension-provided interaction capability");
    }

    for (const auto& pack : CollectProviderContentPacks(*match)) {
        AddUniqueOption(options,
                        seen,
                        "Content Pack: " + pack,
                        "contentpack:" + pack,
                        "Extension-provided content pack");
    }

    for (const auto& endpoint : ReadStringArrayAtPath(match->DescriptorJson, {"optional", "healthEndpoints"})) {
        AddUniqueOption(options,
                        seen,
                        "Health Endpoint: " + endpoint,
                        "health:" + endpoint,
                        "Provider health integration endpoint");
    }

    for (const auto& endpoint : ReadStringArrayAtPath(match->DescriptorJson, {"optional", "contentEndpoints"})) {
        AddUniqueOption(options,
                        seen,
                        "Content Endpoint: " + endpoint,
                        "content:" + endpoint,
                        "Provider content integration endpoint");
    }

    return options;
}

const Core::ISignalNotificationBus* ResolveSignalNotificationBus() {
    auto& registry = Core::ExtensionRegistry::Instance();
    const auto descriptors = registry.ListExtensionDescriptors();

    const Core::ISignalNotificationBus* selected = nullptr;
    int selectedPriority = std::numeric_limits<int>::lowest();

    for (const auto& descriptor : descriptors) {
        auto* instance = registry.GetLoadedExtensionInstance(descriptor.id);
        if (!instance) {
            continue;
        }

        auto* bus = dynamic_cast<Core::ISignalNotificationBus*>(instance);
        if (!bus) {
            continue;
        }

        const int priority = bus->GetSignalNotificationBusPriority();
        if (!selected || priority > selectedPriority) {
            selected = bus;
            selectedPriority = priority;
        }
    }

    return selected;
}

const Core::IInstanceConnectivityProvider* ResolveConnectivityProvider(std::string& outProviderId) {
    outProviderId.clear();

    auto& registry = Core::ExtensionRegistry::Instance();
    const auto descriptors = registry.ListExtensionDescriptors();

    const Core::IInstanceConnectivityProvider* selected = nullptr;
    int selectedPriority = std::numeric_limits<int>::lowest();

    for (const auto& descriptor : descriptors) {
        auto* instance = registry.GetLoadedExtensionInstance(descriptor.id);
        if (!instance) {
            continue;
        }

        auto* provider = dynamic_cast<Core::IInstanceConnectivityProvider*>(instance);
        if (!provider) {
            continue;
        }

        const int priority = provider->GetInstanceConnectivityPriority();
        if (!selected || priority > selectedPriority) {
            selected = provider;
            selectedPriority = priority;
            outProviderId = descriptor.id;
        }
    }

    return selected;
}

CanvasCoreResolveResult ResolveRequirementForCanvasCore(const CanvasCoreResolveRequest& request) {
    CanvasCoreResolveResult result;
    std::vector<CanvasCoreResolveOption> options;

    const std::string providerId = ReadContextValue(request, "providerId");
    const std::string contentPackId = ReadContextValue(request, "contentPackId");

    if (request.RequirementKey == "catalog.extensions") {
        options = BuildInstalledProviderOptions(EProviderScope::Any);
    } else if (request.RequirementKey == "catalog.orchestrators") {
        options = BuildInstalledProviderOptions(EProviderScope::Orchestrator);
    } else if (request.RequirementKey == "catalog.services") {
        options = BuildInstalledProviderOptions(EProviderScope::Service);
    } else if (request.RequirementKey == "catalog.contentpacks.byProvider") {
        options = BuildContentPackOptions(providerId);
    } else if (request.RequirementKey == "catalog.interactions.byProvider") {
        const auto snapshots = CollectDescriptorSnapshots();
        auto match = std::find_if(snapshots.begin(), snapshots.end(), [&providerId](const FDescriptorSnapshot& descriptor) {
            return descriptor.Descriptor.id == providerId;
        });

        if (match != snapshots.end() && IsOrchestratorDescriptorPath(match->DescriptorPath)) {
            options = BuildOrchestratorInteractionOptions(providerId, contentPackId);
        } else {
            options = BuildServiceInteractionOptions(providerId, contentPackId);
        }
    } else if (request.RequirementKey == "catalog.interactions.byContentPack") {
        const auto snapshots = CollectDescriptorSnapshots();
        auto match = std::find_if(snapshots.begin(), snapshots.end(), [&providerId](const FDescriptorSnapshot& descriptor) {
            return descriptor.Descriptor.id == providerId;
        });

        if (match != snapshots.end() && IsOrchestratorDescriptorPath(match->DescriptorPath)) {
            options = BuildOrchestratorInteractionOptions(providerId, contentPackId);
        } else {
            options = BuildServiceInteractionOptions(providerId, contentPackId);
        }
    } else if (request.RequirementKey == "help.catalog.endpoints") {
        options = BuildHelpCatalogEndpointOptions();
    } else if (request.RequirementKey == "help.topics.byProvider") {
        options = BuildHelpTopicOptions(providerId);
    } else {
        result.Success = false;
        result.ErrorCode = "UnsupportedRequirement";
        result.ErrorMessage = "CanvasCore resolver does not support requirement key '" + request.RequirementKey + "'.";
        return result;
    }

    if (options.empty()) {
        result.Success = false;
        result.ErrorCode = "NoOptions";
        result.ErrorMessage = "No options were available for requirement key '" + request.RequirementKey + "'.";
        return result;
    }

    result.Success = true;
    result.Options = std::move(options);
    return result;
}

extern "C" CANVASCORE_CABI_EXPORT bool CanvasCore_ResolveRequirement(const void* requestPtr, void* resultPtr) {
    return Core::RequirementResolver::DispatchResolveRequest(requestPtr, resultPtr, ResolveRequirementForCanvasCore);
}

} // namespace

CanvasCoreModule::CanvasCoreModule() : Runtime_(std::make_unique<CanvasCore::CanvasMenuRuntime>()) {}
CanvasCoreModule::~CanvasCoreModule() {}

void CanvasCoreModule::StartupModule() {
    NOVA_LOG("[CanvasCore] StartupModule starting...", LogType::Log);
    
    // We defer ReloadMenuDefinitions until first use (lazy load) or main loop start.
    // This breaks the recursive initialization cycle where CanvasCore would try
    // to resolve menu meta from other extensions while the registry was still
    // loading them.
    
    std::string startupMessage = "[CanvasCore] StartupModule called. Ready.";
    NOVA_LOG(startupMessage.c_str(), LogType::Log);

    Core::CanvasToastNotification startupToast;
    startupToast.SourceExtensionId = "canvascore";
    startupToast.Title = "CanvasCore active";
    startupToast.Message = "Canvas status and notification surfaces are available.";
    startupToast.Severity = Core::CanvasNotificationSeverity::Info;
    startupToast.DisplayDurationMs = 3000;
    QueueToast(std::move(startupToast));

    //@TODO remove later, just for testing
    /*Core::CanvasToastNotification testToast;
    testToast.SourceExtensionId = "canvascore";
    testToast.Title = "Arcade System Boot";
    testToast.Message = "Testing 8-bit notification queueing... [1/2]";
    testToast.Severity = Core::CanvasNotificationSeverity::Success;
    testToast.DisplayDurationMs = 4000;
    QueueToast(std::move(testToast));

    Core::CanvasToastNotification testToast2;
    testToast2.SourceExtensionId = "canvascore";
    testToast2.Title = "Queue Verification";
    testToast2.Message = "Second notification sliding in! [2/3]";
    testToast2.Severity = Core::CanvasNotificationSeverity::Info;
    testToast2.DisplayDurationMs = 4000;
    QueueToast(std::move(testToast2));
    
    Core::CanvasToastNotification testToast3;
    testToast3.SourceExtensionId = "canvascore";
    testToast3.Title = "SYSTEM_INTERRUPT";
    testToast3.Message = "Manual acknowledgement required. Click [OK] to proceed. [3/3]";
    testToast3.Severity = Core::CanvasNotificationSeverity::Warning;
    testToast3.bRequireAcknowledge = true;
    QueueToast(std::move(testToast3));

    Core::CanvasToastNotification testToast4;
    testToast4.SourceExtensionId = "canvascore";
    testToast4.Title = "Queue Resumed";
    testToast4.Message = "Acknowledgement successful. Resuming normal operations... [4/4]";
    testToast4.Severity = Core::CanvasNotificationSeverity::Success;
    testToast4.DisplayDurationMs = 4000;
    QueueToast(std::move(testToast4));

    Core::CanvasToastNotification testToast5;
    testToast5.SourceExtensionId = "canvascore";
    testToast5.Title = "Sequence Finalized";
    testToast5.Message = "Full notification lifecycle verified. System nominal. [5/5]";
    testToast5.Severity = Core::CanvasNotificationSeverity::Info;
    testToast5.DisplayDurationMs = 4000;
    QueueToast(std::move(testToast5));*/
}

void CanvasCoreModule::ShutdownModule() {
    {
        std::lock_guard<std::mutex> lock(UiStateMutex_);
        ToastQueue_.clear();
        PersistentInfosByMenu_.clear();
        PersistentInfoKeys_.clear();
        LastObservedSignalSequence_ = 0;
        RuntimeNotificationCounter_ = 0;
    }

    Runtime_.reset();
    NOVA_LOG("[CanvasCore] ShutdownModule called", LogType::Log);
}

bool CanvasCoreModule::ReloadMenuDefinitions(std::vector<Core::FJsonParseIssue>& outIssues) {
    if (!Runtime_) {
        Runtime_ = std::make_unique<CanvasCore::CanvasMenuRuntime>();
    }

    ClearPersistentInfosForMenu(kGlobalCanvasMenuId);

    const bool loaded = Runtime_->ReloadMenuDefinitions(outIssues);
    HasReloadedOnFirstUse_ = true;

    int errorCount = 0;
    int warningCount = 0;
    for (const auto& issue : outIssues) {
        if (issue.Severity != Core::EJsonParseSeverity::Error &&
            issue.Severity != Core::EJsonParseSeverity::Warning) {
            continue;
        }

        const std::string issueCode =
            issue.Severity == Core::EJsonParseSeverity::Error
                ? "MenuDefinitionError"
                : "MenuDefinitionWarning";

        RecordPersistentInfo(
            kGlobalCanvasMenuId,
            "",
            issueCode,
            issue.JsonPath + " - " + issue.Message,
            "canvas.menuDefinitions");

        if (issue.Severity == Core::EJsonParseSeverity::Error) {
            ++errorCount;
        } else {
            ++warningCount;
        }
    }

    if (errorCount > 0 || warningCount > 0) {
        Core::CanvasToastNotification toast;
        toast.SourceExtensionId = "canvascore";
        toast.Title = "Canvas menu diagnostics";
        toast.Message = std::to_string(errorCount) + " error(s), " +
                        std::to_string(warningCount) + " warning(s) while loading menu definitions.";
        toast.Severity = errorCount > 0 ? Core::CanvasNotificationSeverity::Error
                                        : Core::CanvasNotificationSeverity::Warning;
        toast.DisplayDurationMs = 6500;
        QueueToast(std::move(toast));
    }

    return loaded;
}

std::vector<std::string> CanvasCoreModule::ListMenuIds() const {
    if (!HasReloadedOnFirstUse_) {
        std::vector<Core::FJsonParseIssue> issues;
        const_cast<CanvasCoreModule*>(this)->ReloadMenuDefinitions(issues);
        HasReloadedOnFirstUse_ = true;
    }
    return Runtime_ ? Runtime_->ListMenuIds() : std::vector<std::string>{};
}

bool CanvasCoreModule::GetMenuDefinition(const std::string& menuId,
                                         CanvasCore::MenuSchema::FCanvasMenuDefinition& outMenu) const {
    if (!HasReloadedOnFirstUse_) {
        std::vector<Core::FJsonParseIssue> issues;
        const_cast<CanvasCoreModule*>(this)->ReloadMenuDefinitions(issues);
        HasReloadedOnFirstUse_ = true;
    }
    return Runtime_ ? Runtime_->GetMenuDefinition(menuId, outMenu) : false;
};

CanvasCore::MenuSchema::FCanvasRequirementResolveResult CanvasCoreModule::ResolveFieldRequirement(
    const std::string& menuId,
    const std::string& fieldId,
    const std::string& consumerExtensionId,
    const std::vector<CanvasCore::FCanvasFieldValue>& contextValues) const {
    PumpSignalNotifications();

    if (!Runtime_) {
        CanvasCore::MenuSchema::FCanvasRequirementResolveResult result;
        result.Success = false;
        result.ErrorCode = "RuntimeUnavailable";
        result.ErrorMessage = "CanvasCore runtime is not initialized.";
        RecordPersistentInfo(menuId, fieldId, result.ErrorCode, result.ErrorMessage, "canvas.requirementResolver");

        Core::CanvasToastNotification toast;
        toast.SourceExtensionId = "canvascore";
        toast.TargetMenuId = NormalizeMenuId(menuId);
        toast.TargetFieldId = fieldId;
        toast.Title = "Canvas resolver offline";
        toast.Message = result.ErrorMessage;
        toast.Severity = Core::CanvasNotificationSeverity::Error;
        QueueToast(std::move(toast));
        return result;
    }

    auto result = Runtime_->ResolveFieldRequirement(menuId, fieldId, consumerExtensionId, contextValues);
    if (!result.Success) {
        const std::string code = result.ErrorCode.empty() ? "RequirementResolveFailed" : result.ErrorCode;
        RecordPersistentInfo(menuId, fieldId, code, result.ErrorMessage, "canvas.requirementResolver");

        Core::CanvasToastNotification toast;
        toast.SourceExtensionId = "canvascore";
        toast.TargetMenuId = NormalizeMenuId(menuId);
        toast.TargetFieldId = fieldId;
        toast.Title = "Canvas resolver issue";
        toast.Message = result.ErrorMessage;
        toast.Severity =
            (code == "ResolverNotFound" || code == "ResolverInvocationFailed")
                ? Core::CanvasNotificationSeverity::Error
                : Core::CanvasNotificationSeverity::Warning;
        toast.DisplayDurationMs = 5200;
        QueueToast(std::move(toast));
    } else {
        ClearPersistentInfosForField(menuId, fieldId);
    }

    return result;
}

bool CanvasCoreModule::BuildSubmitPayload(const std::string& menuId,
                                          const std::vector<CanvasCore::FCanvasFieldValue>& collectedValues,
                                          std::string& outPayloadJson,
                                          std::string& outError) const {
    PumpSignalNotifications();

    if (!Runtime_) {
        outError = "CanvasCore runtime is not initialized.";
        RecordPersistentInfo(menuId, "", "RuntimeUnavailable", outError, "canvas.payloadBuilder");

        Core::CanvasToastNotification toast;
        toast.SourceExtensionId = "canvascore";
        toast.TargetMenuId = NormalizeMenuId(menuId);
        toast.Title = "Canvas submit unavailable";
        toast.Message = outError;
        toast.Severity = Core::CanvasNotificationSeverity::Error;
        QueueToast(std::move(toast));
        return false;
    }

    const bool success = Runtime_->BuildSubmitPayload(menuId, collectedValues, outPayloadJson, outError);
    if (!success) {
        RecordPersistentInfo(menuId, "", "SubmitPayloadBuildFailed", outError, "canvas.payloadBuilder");

        Core::CanvasToastNotification toast;
        toast.SourceExtensionId = "canvascore";
        toast.TargetMenuId = NormalizeMenuId(menuId);
        toast.Title = "Canvas submit payload failed";
        toast.Message = outError;
        toast.Severity = Core::CanvasNotificationSeverity::Error;
        toast.DisplayDurationMs = 5600;
        QueueToast(std::move(toast));
    }

    return success;
}

bool CanvasCoreModule::BuildMenuRenderFrame(const std::string& menuId,
                                            const std::size_t maxToasts,
                                            CanvasCore::FCanvasMenuRenderFrame& outFrame,
                                            std::string& outError) const {
    PumpSignalNotifications();

    outError.clear();
    outFrame = CanvasCore::FCanvasMenuRenderFrame{};
    outFrame.RequestedMenuId = menuId;
    outFrame.GeneratedAtUtc = NowUtcIso8601();

    if (!Runtime_) {
        outError = "CanvasCore runtime is not initialized.";
        RecordPersistentInfo(menuId, "", "RuntimeUnavailable", outError, "canvas.renderFrame");
        return false;
    }

    if (!Runtime_->GetMenuDefinition(menuId, outFrame.Menu)) {
        outError = "Menu '" + menuId + "' was not found in the Canvas runtime.";
        RecordPersistentInfo(menuId, "", "MenuNotFound", outError, "canvas.renderFrame");
        return false;
    }

    outFrame.ResolvedMenuId = menuId;
    outFrame.Chrome.StatusPill = GetCanvasStatusPill();
    outFrame.Chrome.PersistentInfos = GetCanvasPersistentInfos(menuId);
    return true;
}

bool CanvasCoreModule::RunCanvasMenuLoop(const std::string& startMenuId,
                                         std::string& outError) {
    outError.clear();

    if (!Runtime_) {
        outError = "CanvasCore runtime is not initialized.";
        return false;
    }

    if (!HasReloadedOnFirstUse_) {
        std::vector<Core::FJsonParseIssue> issues;
        ReloadMenuDefinitions(issues);
    }

    std::string activeMenuId = startMenuId.empty() ? "main" : startMenuId;
    MenuHistory_.clear();
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> menuValuesByMenu;
    auto ignoreMouseEventsUntil = std::chrono::steady_clock::time_point::min();

    using namespace ftxui;
    // Use one stable viewport for the whole session. FitComponent recalculates
    // its dimensions when menus have different content heights, which makes
    // Windows terminal mouse coordinates drift after navigation.
    auto screen = ScreenInteractive::Fullscreen();

    CanvasCore::UnattendedModeManager unattendedManager;
    
    // Initialize redraw callback for this screen session
    {
        std::lock_guard<std::mutex> lock(UiStateMutex_);
        RedrawCallback_ = [&]() {
            screen.PostEvent(ftxui::Event::Custom);
        };
    }

    // Check for unattended payload argument
    auto* cmdOptions = Utils::CommandLineOptions::GetSingletonInstance();
    // Assuming 'payloadPath' might be a registered string option or we check raw args.
    // For now, we'll hook into the pattern used by other options.
    if (cmdOptions->IsOptionRegistered("payload")) {
        // Trigger unattended mode if a payload is detected
        unattendedManager.Trigger("BOOT_PAYLOAD");
    }

    struct FActiveToastState {
        Core::CanvasToastNotification Toast;
        std::chrono::steady_clock::time_point ReceivedAt;
        bool bAcknowledged = false;
        bool bAnimationStarted = false;
    };
    std::vector<FActiveToastState> activeToasts;
    auto nextAvailableTime = std::chrono::steady_clock::now();


    auto menuExists = [&](const std::string& menuId) {
        CanvasCore::MenuSchema::FCanvasMenuDefinition dummy;
        return GetMenuDefinition(menuId, dummy);
    };

    // Load global persistent configuration.
    nlohmann::json globalConfig = LoadGlobalConfig();
    bool disableMousePartyMode = globalConfig.value("disableMousePartyMode", false);

    bool shouldQuit = false;
    while (!shouldQuit) {
        CanvasCore::FCanvasMenuRenderFrame menuFrame;
        std::string frameError;
        if (!BuildMenuRenderFrame(activeMenuId, 4, menuFrame, frameError)) {
            outError = frameError;
            return false;
        }

        auto& menuValues = menuValuesByMenu[activeMenuId];
        for (const auto& section : menuFrame.Menu.Sections) {
            for (const auto& field : section.Fields) {
                if (menuValues.find(field.Id) == menuValues.end()) {
                    bool loadedFromConfig = false;
                    if (field.ValueStorage == CanvasCore::MenuSchema::ECanvasValueStorage::ConfigJson && !field.OutputKey.empty()) {
                        nlohmann::json configVal;
                        if (GetJsonPathValue(globalConfig, field.OutputKey, configVal)) {
                            if (configVal.is_string()) menuValues[field.Id] = configVal.get<std::string>();
                            else if (configVal.is_boolean()) menuValues[field.Id] = configVal.get<bool>() ? "true" : "false";
                            else if (configVal.is_number()) menuValues[field.Id] = configVal.dump();
                            loadedFromConfig = true;
                        }
                    }
                    
                    if (!loadedFromConfig) {
                        menuValues[field.Id] = field.DefaultValue;
                    }
                }
            }
        }

        auto updateInvokeFlags = [&]() {
            if (!Runtime_) {
                return;
            }

            auto setFlag = [&](const std::string& selectionField,
                               const std::string& flagField) {
                const auto it = menuValues.find(selectionField);
                const bool hasMenu = (it != menuValues.end() && Runtime_->HasMenusForExtension(it->second));
                menuValues[flagField] = hasMenu ? "true" : "false";
            };

            if (activeMenuId == "extensions") {
                setFlag("selectedExtension", "selectedExtensionHasMenu");
            } else if (activeMenuId == "services") {
                setFlag("selectedService", "selectedServiceHasMenu");
            } else if (activeMenuId == "orchestrators") {
                setFlag("selectedOrchestrator", "selectedOrchestratorHasMenu");
            }
        };

        updateInvokeFlags();

        std::string navigateToMenuId;
        bool requestBack = false;
        bool requestRebuild = false;
        auto exitCurrentMenu = [&]() {
            // Windows terminals can deliver a second mouse event after a button
            // has already requested a menu transition. Swallow it in the next
            // frame so it cannot activate a control in the newly rendered menu.
            ignoreMouseEventsUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(180);
            screen.ExitLoopClosure()();
        };

        const auto loopStart = std::chrono::steady_clock::now();

        Core::CanvasStatusPillSnapshot status = menuFrame.Chrome.StatusPill;
        std::vector<Core::CanvasPersistentInfoWidget> persistentInfos = menuFrame.Chrome.PersistentInfos;

        std::vector<std::function<void()>> syncValueCallbacks;
        std::vector<std::function<void()>> visibilityCallbacks;

        auto collectContextValues = [&]() {
            std::vector<CanvasCore::FCanvasFieldValue> contextValues;
            contextValues.reserve(menuValues.size());
            for (const auto& entry : menuValues) {
                contextValues.push_back({entry.first, entry.second});
            }
            return contextValues;
        };

        auto collectSubmitValues = [&]() {
            for (const auto& syncCallback : syncValueCallbacks) {
                syncCallback();
            }

            std::vector<CanvasCore::FCanvasFieldValue> values;
            values.reserve(menuValues.size());
            for (const auto& entry : menuValues) {
                values.push_back({entry.first, entry.second});
            }
            return values;
        };

        auto makePixelButton = [&](const std::string& label,
                                   const std::string& description,
                                   const std::function<void()>& callback,
                                   Color bg,
                                   Color fg,
                                   Color bgActive,
                                   Color fgActive) {
            auto option = ButtonOption::Animated(bg, fg, bgActive, fgActive);
            option.transform = [description](const EntryState& state) {
                const auto now = std::chrono::steady_clock::now().time_since_epoch();
                const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
                bool blink = (ms / 300) % 2 == 0;

                auto label_element = text(state.label) | bold;
                if (state.focused) {
                    label_element = hbox({
                        text(blink ? " > " : "   ") | color(Color::Yellow),
                        label_element | color(Color::White) | inverted,
                        text(blink ? " < " : "   ") | color(Color::Yellow),
                    });
                } else {
                    label_element = text(" [ " + state.label + " ] ");
                }

                auto base = vbox({
                    label_element | center,
                });

                if (!description.empty() && state.focused) {
                    base = vbox({
                        base,
                        text("  ░▒ " + description) | color(Color::GrayLight) | size(HEIGHT, EQUAL, 1),
                    });
                }

                return base | center | borderStyled(state.focused ? BorderStyle::DOUBLE : BorderStyle::LIGHT) | 
                       color(state.focused ? Color::Yellow : Color::GrayDark);
            };
            return Button(label, callback, option);
        };

        Components contentComponents;

        auto isInvokeVisibilityFlag = [](const std::string& fieldId) {
            return fieldId == "selectedExtensionHasMenu" ||
                   fieldId == "selectedServiceHasMenu" ||
                   fieldId == "selectedOrchestratorHasMenu";
        };

        auto computeHasMenuForFlag = [&](const std::string& flagField) {
            if (!Runtime_) {
                return false;
            }

            std::string selectionField;
            if (flagField == "selectedExtensionHasMenu") {
                selectionField = "selectedExtension";
            } else if (flagField == "selectedServiceHasMenu") {
                selectionField = "selectedService";
            } else if (flagField == "selectedOrchestratorHasMenu") {
                selectionField = "selectedOrchestrator";
            }

            if (selectionField.empty()) {
                return false;
            }

            const auto it = menuValues.find(selectionField);
            if (it == menuValues.end() || it->second.empty()) {
                return false;
            }

            return Runtime_->HasMenusForExtension(it->second);
        };

        for (const auto& section : menuFrame.Menu.Sections) {
            contentComponents.push_back(Renderer([section] {
                Elements rows;
                rows.push_back(text(section.Title.empty() ? std::string("Section") : section.Title) | color(Color::Yellow) | bold);
                if (!section.Description.empty()) {
                    rows.push_back(paragraph(section.Description) | color(Color::GrayLight));
                }
                return vbox(std::move(rows));
            }));

            for (const auto& field : section.Fields) {
                if (!field.VisibleIfField.empty() && !isInvokeVisibilityFlag(field.VisibleIfField)) {
                    std::string visibleValue;
                    const auto visibleMatch = menuValues.find(field.VisibleIfField);
                    if (visibleMatch != menuValues.end()) {
                        visibleValue = visibleMatch->second;
                    }
                    if (visibleValue != field.VisibleIfEquals) {
                        continue;
                    }
                }

                // Simplified label rendering: ActionButtons handle their own labels now.
                if (field.Type != CanvasCore::MenuSchema::ECanvasFieldType::Separator &&
                    field.Type != CanvasCore::MenuSchema::ECanvasFieldType::Spacer &&
                    field.Type != CanvasCore::MenuSchema::ECanvasFieldType::ActionButton) {
                    contentComponents.push_back(Renderer([label = field.Label, description = field.Description] {
                        Elements rows;
                        rows.push_back(hbox({
                            text(" ▓ ") | color(Color::Yellow),
                            text(label.empty() ? std::string("FIELD_ID") : label) | bold,
                        }));
                        if (!description.empty()) {
                            rows.push_back(text("   ░▒ " + description) | color(Color::GrayDark));
                        }
                        return vbox(std::move(rows));
                    }));
                }

                using CanvasCore::MenuSchema::ECanvasFieldType;
                using CanvasCore::MenuSchema::ECanvasValidationKind;

                switch (field.Type) {
                    case ECanvasFieldType::ActionButton: {
                        std::string actionTarget = field.DefaultValue;
                        const bool usesInvokeVisibility = isInvokeVisibilityFlag(field.VisibleIfField);
                        auto invokeVisible = std::make_shared<bool>(true);
                        if (usesInvokeVisibility) {
                            *invokeVisible = computeHasMenuForFlag(field.VisibleIfField);
                            visibilityCallbacks.push_back([&, flagField = field.VisibleIfField, invokeVisible] {
                                *invokeVisible = computeHasMenuForFlag(flagField);
                                menuValues[flagField] = *invokeVisible ? "true" : "false";
                            });
                        }

                        auto buttonComponent = makePixelButton(
                            field.Label.empty() ? std::string("RUN_EXEC") : field.Label,
                            field.Description,
                            [&, actionTarget, fieldId = field.Id]() {
                                // Inputs and selectors keep their state in FTXUI-owned values until
                                // the next render.  A direct action must synchronise those values
                                // before it builds its request; otherwise the provider sees an empty
                                // or stale selection when the user clicks immediately after choosing it.
                                for (const auto& syncCallback : syncValueCallbacks) {
                                    syncCallback();
                                }
                                menuValues[fieldId] = actionTarget;

                                if (actionTarget == "test_success") unattendedManager.Trigger("SUCCESS");
                                else if (actionTarget == "test_warning") unattendedManager.Trigger("WARNING");
                                else if (actionTarget == "test_fail") unattendedManager.Trigger("FAIL");
                                else if (actionTarget == "invoke") {
                                    std::string targetExtensionId;
                                    if (activeMenuId == "extensions") {
                                        auto it = menuValues.find("selectedExtension");
                                        if (it != menuValues.end()) targetExtensionId = it->second;
                                    } else if (activeMenuId == "services") {
                                        auto it = menuValues.find("selectedService");
                                        if (it != menuValues.end()) targetExtensionId = it->second;
                                    } else if (activeMenuId == "orchestrators") {
                                        auto it = menuValues.find("selectedOrchestrator");
                                        if (it != menuValues.end()) targetExtensionId = it->second;
                                    }

                                    std::string targetMenuId;
                                    if (!targetExtensionId.empty() && Runtime_) {
                                        targetMenuId = Runtime_->GetDefaultMenuIdForExtension(targetExtensionId);
                                    }

                                    if (!targetMenuId.empty() && menuExists(targetMenuId)) {
                                        navigateToMenuId = targetMenuId;
                                    } else {
                                        Core::CanvasToastNotification toast;
                                        toast.SourceExtensionId = "canvascore";
                                        toast.TargetMenuId = NormalizeMenuId(activeMenuId);
                                        toast.Title = "MENU_UNAVAILABLE";
                                        toast.Message = "No menu definitions were found for the selected extension.";
                                        toast.Severity = Core::CanvasNotificationSeverity::Warning;
                                        PublishCanvasToast(toast);
                                    }
                                }
                                else if (!actionTarget.empty() && (actionTarget == "menu.back" || menuExists(actionTarget))) {
                                    navigateToMenuId = actionTarget;
                                } else if (!actionTarget.empty()) {
                                    // Dispatch to IMenuActionProvider
                                    Core::CanvasMenuActionRequest actionReq;
                                    actionReq.MenuId = activeMenuId;
                                    actionReq.ActionId = actionTarget;
                                    for (const auto& entry : menuValues) {
                                        actionReq.ContextValues[entry.first] = entry.second;
                                    }

                                    auto& registry = Core::ExtensionRegistry::Instance();
                                    const auto descriptors = registry.ListExtensionDescriptors();
                                    const std::string ownerExtensionId = Runtime_ ? Runtime_->GetMenuOwnerExtensionId(activeMenuId) : std::string();

                                    // Menu definitions are discoverable before their owner library is
                                    // loaded.  Direct actions must therefore activate that owner on
                                    // demand instead of silently finding no provider.
                                    if (!ownerExtensionId.empty() && !registry.IsExtensionLoaded(ownerExtensionId)) {
                                        registry.LoadExtensionById(ownerExtensionId);
                                    }

                                    bool actionHandled = false;

                                    for (const auto& descriptor : descriptors) {
                                        if (!ownerExtensionId.empty() && descriptor.id != ownerExtensionId) {
                                            continue;
                                        }
                                        auto* instance = registry.GetLoadedExtensionInstance(descriptor.id);
                                        if (instance) {
                                            auto* actionProvider = dynamic_cast<Core::IMenuActionProvider*>(instance);
                                            if (actionProvider) {
                                                Core::CanvasMenuActionResult actionResult = actionProvider->OnMenuAction(actionReq);
                                                if (actionResult.Success) {
                                                    actionHandled = true;
                                                    for (const auto& updateEntry : actionResult.ConfigUpdates) {
                                                        menuValues[updateEntry.first] = updateEntry.second;
                                                    }
                                                    if (!actionResult.NavigateToMenuId.empty() && (actionResult.NavigateToMenuId == "menu.back" || menuExists(actionResult.NavigateToMenuId))) {
                                                        navigateToMenuId = actionResult.NavigateToMenuId;
                                                    }
                                                } else if (!actionResult.ErrorMessage.empty()) {
                                                    actionHandled = true;
                                                    Core::CanvasToastNotification errToast;
                                                    errToast.SourceExtensionId = descriptor.id;
                                                    errToast.TargetMenuId = NormalizeMenuId(activeMenuId);
                                                    errToast.Title = "ACTION_FAILED";
                                                    errToast.Message = actionResult.ErrorMessage;
                                                    errToast.Severity = Core::CanvasNotificationSeverity::Error;
                                                    PublishCanvasToast(errToast);
                                                }
                                            }
                                        }
                                    }

                                    if (!actionHandled) {
                                        Core::CanvasToastNotification errToast;
                                        errToast.SourceExtensionId = "canvascore";
                                        errToast.TargetMenuId = NormalizeMenuId(activeMenuId);
                                        errToast.Title = "ACTION_UNAVAILABLE";
                                        errToast.Message = "No loaded extension can handle action '" + actionTarget + "'.";
                                        errToast.Severity = Core::CanvasNotificationSeverity::Error;
                                        PublishCanvasToast(errToast);
                                    }
                                }

                                requestRebuild = true;
                                exitCurrentMenu();
                            },
                            Color::Black,
                            Color::Cyan,
                            Color::Cyan,
                            Color::Black);

                        if (usesInvokeVisibility) {
                            contentComponents.push_back(Maybe(buttonComponent, invokeVisible.get()));
                        } else {
                            contentComponents.push_back(buttonComponent);
                        }
                        break;
                    }

                    case ECanvasFieldType::ProviderSelect:
                    case ECanvasFieldType::Dropdown:
                    case ECanvasFieldType::RadioGroup:
                    case ECanvasFieldType::ToggleGroup: {
                        auto resolveResult = ResolveFieldRequirement(
                            activeMenuId,
                            field.Id,
                            "canvascore",
                            collectContextValues());

                        if (!resolveResult.Success || resolveResult.Options.empty()) {
                            std::string errorLine = resolveResult.ErrorMessage.empty()
                                ? "Resolver returned no options."
                                : resolveResult.ErrorMessage;
                            contentComponents.push_back(Renderer([errorLine] {
                                return text("[resolver] " + errorLine) | color(Color::Red);
                            }));
                            break;
                        }

                        auto labels = std::make_shared<std::vector<std::string>>();
                        auto values = std::make_shared<std::vector<std::string>>();
                        labels->reserve(resolveResult.Options.size());
                        values->reserve(resolveResult.Options.size());

                        for (const auto& option : resolveResult.Options) {
                            labels->push_back(option.Label.empty() ? option.Value : option.Label);
                            values->push_back(option.Value);
                        }

                        int selectedIndex = 0;
                        const auto selectedValue = menuValues.find(field.Id);
                        if (selectedValue != menuValues.end() && !selectedValue->second.empty()) {
                            for (std::size_t index = 0; index < values->size(); ++index) {
                                if ((*values)[index] == selectedValue->second) {
                                    selectedIndex = static_cast<int>(index);
                                    break;
                                }
                            }
                        } else if (!values->empty()) {
                            menuValues[field.Id] = (*values)[0];
                        }

                        auto selected = std::make_shared<int>(selectedIndex);
                        syncValueCallbacks.push_back([selected, values, &menuValues, fieldId = field.Id] {
                            if (values->empty()) {
                                menuValues[fieldId].clear();
                                return;
                            }

                            int safeIndex = *selected;
                            if (safeIndex < 0 || safeIndex >= static_cast<int>(values->size())) {
                                safeIndex = 0;
                            }
                            menuValues[fieldId] = (*values)[static_cast<std::size_t>(safeIndex)];
                        });

                        if (field.Type == ECanvasFieldType::RadioGroup) {
                            contentComponents.push_back(Radiobox(*labels, selected.get()));
                        } else if (field.Type == ECanvasFieldType::ToggleGroup) {
                            contentComponents.push_back(Toggle(*labels, selected.get()));
                        } else {
                            contentComponents.push_back(Dropdown(*labels, selected.get()));
                        }
                        break;
                    }

                    case ECanvasFieldType::ProviderMultiSelect: {
                        auto resolveResult = ResolveFieldRequirement(
                            activeMenuId,
                            field.Id,
                            "canvascore",
                            collectContextValues());

                        if (!resolveResult.Success || resolveResult.Options.empty()) {
                            std::string errorLine = resolveResult.ErrorMessage.empty()
                                ? "Resolver returned no options."
                                : resolveResult.ErrorMessage;
                            contentComponents.push_back(Renderer([errorLine] {
                                return text("[resolver] " + errorLine) | color(Color::Red);
                            }));
                            break;
                        }

                        std::unordered_map<std::string, bool> selectedValues;
                        const auto current = menuValues.find(field.Id);
                        if (current != menuValues.end()) {
                            std::stringstream stream(current->second);
                            std::string token;
                            while (std::getline(stream, token, ',')) {
                                if (!token.empty()) {
                                    selectedValues[token] = true;
                                }
                            }
                        }

                        auto checkStates = std::make_shared<std::vector<std::shared_ptr<bool>>>();
                        Components multiSelectRows;

                        for (const auto& option : resolveResult.Options) {
                            auto checked = std::make_shared<bool>(selectedValues.find(option.Value) != selectedValues.end());
                            checkStates->push_back(checked);
                            multiSelectRows.push_back(Checkbox(option.Label.empty() ? option.Value : option.Label, checked.get()));
                        }

                        syncValueCallbacks.push_back([
                            checkStates,
                            resolveResult,
                            &menuValues,
                            fieldId = field.Id]() {
                            std::string joined;
                            for (std::size_t index = 0; index < checkStates->size(); ++index) {
                                if (!(*checkStates)[index] || !*(*checkStates)[index]) {
                                    continue;
                                }
                                if (!joined.empty()) {
                                    joined += ",";
                                }
                                joined += resolveResult.Options[index].Value;
                            }
                            menuValues[fieldId] = std::move(joined);
                        });

                        contentComponents.push_back(Container::Vertical(std::move(multiSelectRows)));
                        break;
                    }

                    case ECanvasFieldType::Bool: {
                        auto checked = std::make_shared<bool>(menuValues[field.Id] == "true" || menuValues[field.Id] == "1");
                        syncValueCallbacks.push_back([checked, &menuValues, fieldId = field.Id] {
                            menuValues[fieldId] = *checked ? "true" : "false";
                        });
                        contentComponents.push_back(Checkbox(field.Label.empty() ? std::string("Enabled") : field.Label, checked.get()));
                        break;
                    }

                    case ECanvasFieldType::String:
                    case ECanvasFieldType::PasswordString:
                    case ECanvasFieldType::Integer:
                    case ECanvasFieldType::Float: {
                        auto inputValue = std::make_shared<std::string>(menuValues[field.Id]);
                        InputOption inputOption = InputOption::Default();
                        inputOption.placeholder = field.Description.empty() ? field.Label : field.Description;
                        inputOption.password = field.Type == ECanvasFieldType::PasswordString;
                        inputOption.multiline = false;

                        syncValueCallbacks.push_back([inputValue, &menuValues, fieldId = field.Id] {
                            menuValues[fieldId] = *inputValue;
                        });

                        contentComponents.push_back(Input(inputValue.get(), inputOption));
                        break;
                    }

                    case ECanvasFieldType::SliderInt: {
                        int minValue = 0;
                        int maxValue = 100;
                        int increment = 1;
                        for (const auto& validation : field.Validations) {
                            if (validation.Kind == ECanvasValidationKind::Min) {
                                minValue = static_cast<int>(ParseDoubleOrDefault(validation.Argument, minValue));
                            } else if (validation.Kind == ECanvasValidationKind::Max) {
                                maxValue = static_cast<int>(ParseDoubleOrDefault(validation.Argument, maxValue));
                            }
                        }

                        auto sliderValue = std::make_shared<int>(static_cast<int>(ParseDoubleOrDefault(menuValues[field.Id], field.DefaultValue.empty() ? minValue : ParseDoubleOrDefault(field.DefaultValue, minValue))));
                        syncValueCallbacks.push_back([sliderValue, &menuValues, fieldId = field.Id] {
                            menuValues[fieldId] = std::to_string(*sliderValue);
                        });

                        contentComponents.push_back(Slider(field.Label, sliderValue.get(), minValue, maxValue, increment));
                        break;
                    }

                    case ECanvasFieldType::SliderFloat: {
                        float minValue = 0.0f;
                        float maxValue = 100.0f;
                        float increment = 1.0f;
                        for (const auto& validation : field.Validations) {
                            if (validation.Kind == ECanvasValidationKind::Min) {
                                minValue = static_cast<float>(ParseDoubleOrDefault(validation.Argument, minValue));
                            } else if (validation.Kind == ECanvasValidationKind::Max) {
                                maxValue = static_cast<float>(ParseDoubleOrDefault(validation.Argument, maxValue));
                            }
                        }

                        auto sliderValue = std::make_shared<float>(static_cast<float>(ParseDoubleOrDefault(menuValues[field.Id], field.DefaultValue.empty() ? minValue : ParseDoubleOrDefault(field.DefaultValue, minValue))));
                        syncValueCallbacks.push_back([sliderValue, &menuValues, fieldId = field.Id] {
                            std::ostringstream valueStream;
                            valueStream << *sliderValue;
                            menuValues[fieldId] = valueStream.str();
                        });

                        contentComponents.push_back(Slider(field.Label, sliderValue.get(), minValue, maxValue, increment));
                        break;
                    }

                    case ECanvasFieldType::TextLabel: {
                        auto textSource = std::make_shared<std::string>();
                        const auto match = menuValues.find(field.Id);
                        *textSource = match == menuValues.end()
                            ? (field.DefaultValue.empty() ? field.Label : field.DefaultValue)
                            : match->second;
                        contentComponents.push_back(Renderer([textSource] { return text(*textSource) | color(Color::White); }));
                        break;
                    }

                    case ECanvasFieldType::Paragraph: {
                        auto textSource = std::make_shared<std::string>();
                        const auto match = menuValues.find(field.Id);
                        if (match != menuValues.end()) {
                            *textSource = match->second;
                        } else {
                            *textSource = field.DefaultValue.empty() ? field.Description : field.DefaultValue;
                        }
                        
                        contentComponents.push_back(Renderer([textSource] { 
                            return paragraph(*textSource) | color(Color::GrayLight); 
                        }));
                        break;
                    }

                    case ECanvasFieldType::TerminalView: {
                        if (field.RequirementBinding.has_value()) {
                            auto resolveResult = ResolveFieldRequirement(
                                activeMenuId,
                                field.Id,
                                "canvascore",
                                collectContextValues());
                            
                            if (resolveResult.Success && !resolveResult.Options.empty()) {
                                menuValues[field.Id] = resolveResult.Options[0].Value;
                            }
                        }

                        auto textSource = std::make_shared<std::string>();
                        const auto match = menuValues.find(field.Id);
                        if (match != menuValues.end()) {
                            *textSource = match->second;
                        } else {
                            *textSource = field.DefaultValue;
                        }

                        contentComponents.push_back(Renderer([textSource] {
                            // Terminal style: monospace (if supported by terminal), vertical scrolling, 8-bit look
                            return vbox({
                                paragraph(*textSource) | color(Color::GreenLight)
                            }) | borderDouble | color(Color::Green) | vscroll_indicator | frame | size(HEIGHT, EQUAL, 12);
                        }));
                        break;
                    }

                    case ECanvasFieldType::Separator:
                        contentComponents.push_back(Renderer([] { return separatorHeavy(); }));
                        break;

                    case ECanvasFieldType::Spacer:
                        contentComponents.push_back(Renderer([] { return text(""); }));
                        break;

                    case ECanvasFieldType::ProgressGauge:
                    case ECanvasFieldType::DirectionalGauge: {
                        contentComponents.push_back(Renderer([
                            &menuValues,
                            fieldId = field.Id,
                            defaultValue = field.DefaultValue,
                            dataRequirementKey = field.DataRequirementKey,
                            label = field.Label,
                            isDirectional = field.Type == ECanvasFieldType::DirectionalGauge,
                            templateName = field.RenderTemplate] {
                            std::string sourceValue = defaultValue;
                            std::string renderedLabel = label;
                            if (dataRequirementKey == "core.progress.percent") {
                                const auto snapshot = Core::ProgressTracker::Read();
                                sourceValue = std::to_string(snapshot.percent);
                                if (!snapshot.phase.empty()) renderedLabel += ": " + snapshot.phase;
                            } else if (const auto valueIt = menuValues.find(fieldId); valueIt != menuValues.end() && !valueIt->second.empty()) {
                                sourceValue = valueIt->second;
                            }
                            double progressValue = ParseDoubleOrDefault(sourceValue, 0.0);
                            if (progressValue > 1.0) progressValue /= 100.0;
                            const float progress = Clamp01(progressValue);
                            Element meter;
                            if (isDirectional) {
                                const std::string lowerTemplate = ToLower(templateName);
                                if (lowerTemplate.find("left") != std::string::npos) {
                                    meter = gaugeLeft(progress);
                                } else if (lowerTemplate.find("up") != std::string::npos) {
                                    meter = gaugeUp(progress);
                                } else if (lowerTemplate.find("down") != std::string::npos) {
                                    meter = gaugeDown(progress);
                                } else {
                                    meter = gaugeRight(progress);
                                }
                            } else {
                                meter = gauge(progress);
                            }

                            return vbox({
                                text(renderedLabel.empty() ? std::string("Gauge") : renderedLabel) | color(Color::White) | bold,
                                meter | color(Color::Green),
                            });
                        }));
                        break;
                    }

                    case ECanvasFieldType::Spinner: {
                        const auto spinnerStart = std::chrono::steady_clock::now();
                        contentComponents.push_back(Renderer([spinnerStart, label = field.Label] {
                            const auto now = std::chrono::steady_clock::now();
                            const auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - spinnerStart).count();
                            const std::size_t frameIndex = static_cast<std::size_t>((ageMs / 120) % 10);
                            return hbox({
                                       text(label.empty() ? std::string("Spinner") : label) | color(Color::White),
                                       text(" "),
                                       spinner(0, frameIndex) | color(Color::Cyan),
                                   }) |
                                   border;
                        }));
                        break;
                    }

                    case ECanvasFieldType::WindowPane: {
                        const std::string body = field.Description.empty()
                            ? std::string("Canvas window pane placeholder")
                            : field.Description;
                        contentComponents.push_back(Renderer([title = field.Label, body] {
                            return window(text(title.empty() ? std::string("Pane") : title), paragraph(body));
                        }));
                        break;
                    }

                    case ECanvasFieldType::CollapsibleSection: {
                        const std::string body = field.Description.empty()
                            ? std::string("Collapsible content")
                            : field.Description;
                        auto child = Renderer([body] { return paragraph(body) | color(Color::GrayLight); });
                        contentComponents.push_back(Collapsible(field.Label.empty() ? std::string("Details") : field.Label, child, false));
                        break;
                    }

                    case ECanvasFieldType::CanvasChart:
                    case ECanvasFieldType::SparklineGraph:
                    case ECanvasFieldType::Table:
                    case ECanvasFieldType::StructObject:
                    case ECanvasFieldType::ResizableSplit:
                    case ECanvasFieldType::TabContainer:
                    default: {
                        const std::string placeholder = "FTXUI element mapped for type: " + FieldTypeLabel(field.Type);
                        contentComponents.push_back(Renderer([placeholder] {
                            return text(placeholder) | color(Color::GrayDark) | italic;
                        }));
                        break;
                    }
                }
            }

            contentComponents.push_back(Renderer([] { return separatorLight(); }));
        }

        if (contentComponents.empty()) {
            contentComponents.push_back(Renderer([] { return text("No renderable fields were found in this menu."); }));
        }

        auto contentContainer = Container::Vertical(std::move(contentComponents));

        auto applyButton = makePixelButton(
            "APPLY",
            "Commit configuration changes to the mesh registry.",
            [&]() {
                auto collectedValues = collectSubmitValues();
                std::string payload;
                std::string buildError;
                if (BuildSubmitPayload(activeMenuId, collectedValues, payload, buildError)) {
                    // Update persistent config for any fields marked with ConfigJson.
                    bool configChanged = false;
                    nlohmann::json payloadJson = nlohmann::json::parse(payload);
                    auto* cmdOptions = Utils::CommandLineOptions::GetSingletonInstance();

                    for (const auto& section : menuFrame.Menu.Sections) {
                        for (const auto& field : section.Fields) {
                            if (field.ValueStorage == CanvasCore::MenuSchema::ECanvasValueStorage::ConfigJson && !field.OutputKey.empty()) {
                                nlohmann::json newVal;
                                if (GetJsonPathValue(payloadJson, field.OutputKey, newVal)) {
                                    SetJsonPathValue(globalConfig, field.OutputKey, newVal);
                                    configChanged = true;

                                    // Sync with CommandLineOptions if registered
                                    std::string valueStr;
                                    if (newVal.is_string()) valueStr = newVal.get<std::string>();
                                    else if (newVal.is_boolean()) valueStr = newVal.get<bool>() ? "true" : "false";
                                    else valueStr = newVal.dump();

                                    if (cmdOptions->IsOptionRegistered(field.Id)) {
                                        cmdOptions->SetOptionValue(field.Id, valueStr);
                                    } else if (cmdOptions->IsOptionRegistered(field.OutputKey)) {
                                        cmdOptions->SetOptionValue(field.OutputKey, valueStr);
                                    }
                                }
                            }
                        }
                    }

                    if (configChanged) {
                        SaveGlobalConfig(globalConfig);
                    }

                    Core::CanvasToastNotification toast;
                    toast.SourceExtensionId = "canvascore";
                    toast.TargetMenuId = NormalizeMenuId(activeMenuId);
                    toast.Title = "CONFIG_SYNC_OK";
                    toast.Message = menuFrame.Menu.SubmitAction.empty()
                        ? "Payload generated from canvas field values."
                        : ("Action '" + menuFrame.Menu.SubmitAction + "' payload generated.");
                    toast.Severity = Core::CanvasNotificationSeverity::Success;
                    PublishCanvasToast(toast);

                    std::string logPayload = "[CanvasCore] submit payload for menu '" + activeMenuId + "':\n" + payload;
                    NOVA_LOG(logPayload.c_str(), LogType::Log);

                    // Dispatch to any IMenuActionProvider
                    if (!menuFrame.Menu.SubmitAction.empty() && menuFrame.Menu.SubmitAction != "custom.unattended_trigger" && !menuExists(menuFrame.Menu.SubmitAction)) {
                        Core::CanvasMenuActionRequest actionReq;
                        actionReq.MenuId = activeMenuId;
                        actionReq.ActionId = menuFrame.Menu.SubmitAction;
                        for (const auto& entry : collectedValues) {
                            actionReq.ContextValues[entry.FieldId] = entry.Value;
                        }

                        auto& registry = Core::ExtensionRegistry::Instance();
                        const auto descriptors = registry.ListExtensionDescriptors();
                        std::string ownerExtensionId;
                        if (Runtime_) {
                            ownerExtensionId = Runtime_->GetMenuOwnerExtensionId(activeMenuId);
                        }

                        if (ownerExtensionId.empty()) {
                            if (activeMenuId == "extensions") {
                                auto it = menuValues.find("selectedExtension");
                                if (it != menuValues.end()) ownerExtensionId = it->second;
                            } else if (activeMenuId == "services") {
                                auto it = menuValues.find("selectedService");
                                if (it != menuValues.end()) ownerExtensionId = it->second;
                            } else if (activeMenuId == "orchestrators") {
                                auto it = menuValues.find("selectedOrchestrator");
                                if (it != menuValues.end()) ownerExtensionId = it->second;
                            }
                        }
                        bool actionHandled = false;

                        for (const auto& descriptor : descriptors) {
                            if (!ownerExtensionId.empty() && descriptor.id != ownerExtensionId) {
                                continue;
                            }
                            auto* instance = registry.GetLoadedExtensionInstance(descriptor.id);
                            if (instance) {
                                auto* actionProvider = dynamic_cast<Core::IMenuActionProvider*>(instance);
                                if (actionProvider) {
                                    Core::CanvasMenuActionResult actionResult = actionProvider->OnMenuAction(actionReq);
                                    if (!actionResult.Success) {
                                        Core::CanvasToastNotification errToast;
                                        errToast.SourceExtensionId = descriptor.id;
                                        errToast.TargetMenuId = NormalizeMenuId(activeMenuId);
                                        errToast.Title = "ACTION_FAILED";
                                        errToast.Message = actionResult.ErrorMessage.empty() ? "The extension rejected the action." : actionResult.ErrorMessage;
                                        errToast.Severity = Core::CanvasNotificationSeverity::Error;
                                        PublishCanvasToast(errToast);
                                    } else {
                                        for (const auto& updateEntry : actionResult.ConfigUpdates) {
                                            menuValues[updateEntry.first] = updateEntry.second;
                                            // also update context config json
                                            for (const auto& section : menuFrame.Menu.Sections) {
                                                for (const auto& field : section.Fields) {
                                                    if (field.Id == updateEntry.first && field.ValueStorage == CanvasCore::MenuSchema::ECanvasValueStorage::ConfigJson && !field.OutputKey.empty()) {
                                                        SetJsonPathValue(globalConfig, field.OutputKey, nlohmann::json(updateEntry.second));
                                                        SaveGlobalConfig(globalConfig);
                                                    }
                                                }
                                            }
                                        }
                                        if (!actionResult.NavigateToMenuId.empty() && (actionResult.NavigateToMenuId == "menu.back" || menuExists(actionResult.NavigateToMenuId))) {
                                            navigateToMenuId = actionResult.NavigateToMenuId;
                                        }
                                    }
                                    actionHandled = true;
                                }
                            }
                        }

                        if (!actionHandled) {
                            // Optionally warn that no one handled it
                        }
                    }

                    // If SubmitAction names a menu, navigate there on success.
                    if (menuFrame.Menu.SubmitAction == "custom.unattended_trigger") {
                        unattendedManager.Trigger("SUCCESS"); // Default if triggered via submit
                        requestRebuild = true;
                    } else if (!menuFrame.Menu.SubmitAction.empty() && (menuFrame.Menu.SubmitAction == "menu.back" || menuExists(menuFrame.Menu.SubmitAction))) {
                        navigateToMenuId = menuFrame.Menu.SubmitAction;
                    } else {
                        requestRebuild = true;
                    }
                } else {
                    Core::CanvasToastNotification toast;
                    toast.SourceExtensionId = "canvascore";
                    toast.TargetMenuId = NormalizeMenuId(activeMenuId);
                    toast.Title = "CONFIG_SYNC_FAIL";
                    toast.Message = buildError;
                    toast.Severity = Core::CanvasNotificationSeverity::Error;
                    PublishCanvasToast(toast);
                    requestRebuild = true;
                }

                exitCurrentMenu();
            },
            Color::Black,
            Color::Green,
            Color::Green,
            Color::Black);

        auto refreshButton = makePixelButton(
            "REFRESH",
            "Reload active menu schema and sync local state.",
            [&]() {
                requestRebuild = true;
                exitCurrentMenu();
            },
            Color::Black,
            Color::Cyan,
            Color::Cyan,
            Color::Black);

        auto backButton = makePixelButton(
            "BACK",
            "Return to the previous navigation node.",
            [&]() {
                // CancelAction in the menu definition overrides the default
                // history-pop behaviour, making navigation fully metadata-driven.
                if (!menuFrame.Menu.CancelAction.empty() && (menuFrame.Menu.CancelAction == "menu.back" || menuExists(menuFrame.Menu.CancelAction))) {
                    navigateToMenuId = menuFrame.Menu.CancelAction;
                } else {
                    requestBack = true;
                }
                exitCurrentMenu();
            },
            Color::Black,
            Color::Yellow,
            Color::Yellow,
            Color::Black);

        auto quitButton = makePixelButton(
            "QUIT",
            "Terminate the current session and exit.",
            [&]() {
                shouldQuit = true;
                exitCurrentMenu();
            },
            Color::Black,
            Color::Red,
            Color::Red,
            Color::White);

        auto actions = Container::Horizontal({applyButton, refreshButton, backButton, quitButton});
        
        // Logical root of the menu application
        auto menuRoot = Container::Vertical({
            contentContainer,
            actions
        });

        // Interactive toast acknowledgement button
        auto ackButton = Button(" [ OK ] ", [&] {
            if (!activeToasts.empty()) {
                auto& t = activeToasts.front();
                t.bAcknowledged = true;
                // Reset timing so the toast starts its fade-out phase immediately
                const int mAge = t.Toast.DisplayDurationMs > 0 ? t.Toast.DisplayDurationMs : 4200;
                t.ReceivedAt = std::chrono::steady_clock::now() - std::chrono::milliseconds(mAge - 300);
            }
        }, ButtonOption::Animated(Color::Black, Color::Green, Color::Green, Color::Black));

        bool showAckButton = false;
        auto maybeAck = Maybe(ackButton, &showAckButton);

        // Overlay layout for interactive components (aligned to top to match toast position)
        auto interactiveOverlay = Renderer(maybeAck, [&] {
            return vbox({
                filler() | size(HEIGHT, EQUAL, 4), // Vertical offset to align with toast content
                maybeAck->Render() | hcenter,
                filler()
            }) | size(HEIGHT, EQUAL, 12);
        });

        auto mainStack = Container::Stacked({
            menuRoot,
            interactiveOverlay
        });

        auto component = Renderer(mainStack, [&]() -> Element {
            showAckButton = false; // Reset state for each frame

            for (const auto& callback : visibilityCallbacks) {
                callback();
            }

            for (const auto& syncCallback : syncValueCallbacks) {
                syncCallback();
            }

            status = GetCanvasStatusPill();
            persistentInfos = GetCanvasPersistentInfos(activeMenuId);

            const auto tNow = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tNow - loopStart).count();

            // Unattended Mode Simulation Loop
            unattendedManager.Update();
            
            // Check if we have an active interactive toast that blocks new ones
            bool bBlockingActive = false;
            for (const auto& toast : activeToasts) {
                if (toast.Toast.bRequireAcknowledge && !toast.bAcknowledged) {
                    bBlockingActive = true;
                    break;
                }
            }

            if (nextAvailableTime < tNow) {
                nextAvailableTime = tNow;
            }

            if (tNow >= nextAvailableTime && !bBlockingActive) {
                const auto newToasts = ConsumeCanvasToastsForMenu(activeMenuId, 3);
                for (const auto& toast : newToasts) {
                    auto startTime = nextAvailableTime;
                    const int mAge = toast.DisplayDurationMs > 0 ? toast.DisplayDurationMs : 4200;
                    
                    // Set the next available slot to after this toast expires + 2 seconds gap
                    // Set the next available slot to after this toast expires + a small gap
                    nextAvailableTime = startTime + std::chrono::milliseconds(mAge + 500);
                    activeToasts.push_back({toast, startTime, false, false});
                }
            }
            
            // Critical fix for the "plop" after long blocking:
            // Ensure nextAvailableTime doesn't trail behind tNow, but also respect
            // any active toast's remaining time so the next one slides in perfectly.
            if (!activeToasts.empty()) {
                const auto& lastToast = activeToasts.back();
                const int lastMaxAge = lastToast.Toast.DisplayDurationMs > 0 ? lastToast.Toast.DisplayDurationMs : 4200;
                auto lastEndTime = lastToast.ReceivedAt + std::chrono::milliseconds(lastMaxAge + 500);
                if (nextAvailableTime < lastEndTime) nextAvailableTime = lastEndTime;
            }

            if (nextAvailableTime < tNow) nextAvailableTime = tNow;

            // Note: We move erase logic later to ensure we don't skip the last frame of animation.

            // 8-bit Rainbow Color Logic
            auto get_rainbow_color = [&](int offset) -> Color {
                int color_cycle = (elapsed / 100) % 360;
                int hue = (color_cycle + offset) % 360;
                if (hue < 60) return Color::Red;
                else if (hue < 120) return Color::Yellow; 
                else if (hue < 180) return Color::Green;
                else if (hue < 240) return Color::Cyan;
                else if (hue < 300) return Color::Blue;
                else return Color::Magenta;
            };

            // Arcade Particle Effect
            auto createParticles = [&]() -> Element {
                if (disableMousePartyMode) return text("");
                Elements particles;
                for (int x = 0; x < 12; ++x) {
                    auto particle_time = (elapsed + x * 137) / 60;
                    auto char_idx = particle_time % 4;
                    std::string p_char = "  ";
                    if (char_idx == 0) p_char = "· ";
                    else if (char_idx == 1) p_char = "○ ";
                    else if (char_idx == 2) p_char = "✦ ";
                    else p_char = "▓ ";
                    particles.push_back(text(p_char) | color(get_rainbow_color(x * 25)));
                }
                return hbox(particles) | center;
            };

            Color statusColor = Color::Yellow;
            if (status.ModeLabel == "HOST") {
                statusColor = Color::Green;
            } else if (status.ModeLabel == "CLIENT") {
                statusColor = Color::Cyan;
            } else if (status.ModeLabel == "LOCAL") {
                statusColor = Color::White;
            }

            Element statusPill = hbox({
                text(" ▓▒░ "),
                text(status.ModeLabel.empty() ? std::string("SYSTEM") : status.ModeLabel) | color(statusColor) | bold,
                text(" | MESH_NODES "),
                text(std::to_string(status.ConnectedInstanceCount)) | color(get_rainbow_color(45)) | bold,
                text(" ░▒▓ "),
            }) | center;

            // Arcade Header
            auto header = vbox({
                createParticles(),
                hbox({
                    text(" [ "),
                    text("CELESTIA NOVA") | bold | color(get_rainbow_color(0)),
                    text(" ] "),
                }) | center,
                hbox({
                    text(" < "),
                    text(menuFrame.Menu.Title.empty() ? "MAIN_BOARD" : menuFrame.Menu.Title) | bold | color(get_rainbow_color(90)),
                    text(" > "),
                }) | center,
                separatorDouble() | color(get_rainbow_color(180)),
            });

            Elements infoRows;
            infoRows.push_back(text(" [ DIAGNOSTIC_BUS ] ") | color(Color::Yellow) | bold);
            if (persistentInfos.empty()) {
                infoRows.push_back(text("  STATUS: GREEN_LIGHT") | color(Color::Green));
            } else {
                const std::size_t visibleInfos = persistentInfos.size() > 3 ? 3 : persistentInfos.size();
                for (std::size_t index = 0; index < visibleInfos; ++index) {
                    const auto& info = persistentInfos[persistentInfos.size() - 1 - index];
                    std::string line = "  ! " + info.Code + ": " + info.Message;
                    if (line.size() > 88) {
                        line = line.substr(0, 85) + "...";
                    }
                    infoRows.push_back(text(line) | color(Color::Red));
                }
            }

            Element notificationLayer = text("");
            bool showNotification = false;
            bool hasActiveNotification = false;
            if (!activeToasts.empty()) {
                auto& toastState = activeToasts.front(); 
                
                // Ensure the animation starts from zero exactly when this toast becomes the front
                if (!toastState.bAnimationStarted) {
                    toastState.ReceivedAt = tNow;
                    toastState.bAnimationStarted = true;
                }

                const auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(tNow - toastState.ReceivedAt).count();
                const int maxAge = toastState.Toast.DisplayDurationMs > 0 ? toastState.Toast.DisplayDurationMs : 4200;
                
                float animScale = 0.0f;
                if (ageMs < 0) {
                    animScale = 0.0f; // Still waiting in queue
                    hasActiveNotification = true;
                } else if (ageMs < 300) {
                    animScale = (float)ageMs / 300.0f;
                    hasActiveNotification = true; 
                } else {
                    // Decide if we should fade out or stay visible
                    bool bShouldFadeOut = false;
                    if (!toastState.Toast.bRequireAcknowledge || toastState.bAcknowledged) {
                        if (ageMs > (maxAge - 300)) {
                            bShouldFadeOut = true;
                        }
                    }

                    if (bShouldFadeOut) {
                        animScale = (float)(maxAge - ageMs) / 300.0f;
                        if (animScale < 0.0f) animScale = 0.0f;
                    } else {
                        animScale = 1.0f; // Sticky or middle of duration
                    }
                    hasActiveNotification = true;
                }

                if (animScale > 0.01f) {
                    showNotification = true;
                    Color severityColor = Color::Cyan;
                    std::string severityTag = "INFO";
                    switch (toastState.Toast.Severity) {
                        case Core::CanvasNotificationSeverity::Success: severityColor = Color::Green; severityTag = "SUCCESS"; break;
                        case Core::CanvasNotificationSeverity::Warning: severityColor = Color::Yellow; severityTag = "WARNING"; break;
                        case Core::CanvasNotificationSeverity::Error: severityColor = Color::Red; severityTag = "FAILURE"; break;
                        default: break;
                    }

                    int estimatedLines = (int)(toastState.Toast.Message.length() / 45) + 1;
                    int targetHeight = 3 + estimatedLines; // Title + Spacer + Wrapped lines
                    if (toastState.Toast.bRequireAcknowledge) targetHeight += 2; // Extra space for button
                    
                    if (targetHeight < 4) targetHeight = 4;
                    if (targetHeight > 12) targetHeight = 12;

                    showAckButton = toastState.Toast.bRequireAcknowledge && animScale > 0.99f && !toastState.bAcknowledged;

                    notificationLayer = vbox({
                        hbox({
                            text(" ⚡ ") | color(severityColor) | bold,
                            text("[" + severityTag + "] ") | bold | color(severityColor),
                            text(toastState.Toast.Title) | bold | color(Color::White),
                        }) | hcenter,
                        separatorEmpty(),
                        paragraph(toastState.Toast.Message) | color(Color::White) | hcenter,
                        showAckButton ? separatorEmpty() : text(""),
                        showAckButton ? (text("") | size(HEIGHT, EQUAL, 1)) : text("") // Keep space for the interactive button
                    }) | size(WIDTH, LESS_THAN, 100) | bgcolor(Color::Black) | clear_under | borderDouble | color(severityColor) | 
                         size(HEIGHT, EQUAL, (int)(animScale * targetHeight));
                    
                    notificationLayer = notificationLayer | hcenter;
                }
            }

            // Perform cleanup of expired toasts after rendering to avoid flickering/jumps.
            activeToasts.erase(
                std::remove_if(activeToasts.begin(), activeToasts.end(), [&](const FActiveToastState& toast) {
                    // Interactive toasts stay until acknowledged
                    if (toast.Toast.bRequireAcknowledge && !toast.bAcknowledged) {
                        return false;
                    }
                    
                    const auto tNow = std::chrono::steady_clock::now();
                    const auto tAgeMs = std::chrono::duration_cast<std::chrono::milliseconds>(tNow - toast.ReceivedAt).count();
                    const int tMaxAge = toast.Toast.DisplayDurationMs > 0 ? toast.Toast.DisplayDurationMs : 4200;
                    return tAgeMs > tMaxAge;
                }),
                activeToasts.end());

            // Keep the arcade UI alive for rainbow cycling and background toast checks
            screen.RequestAnimationFrame();

            auto mainApp = vbox({
                               header,
                               // Do not place interactive fields inside an FTXUI frame. A frame
                               // scrolls the visual tree independently of component event boxes,
                               // which produces vertically offset mouse hit targets.
                               contentContainer->Render() | flex | border | color(get_rainbow_color(270)),
                               separatorLight() | color(get_rainbow_color(300)),
                               hbox({
                                   vbox(std::move(infoRows)) | flex,
                               }) | size(HEIGHT, EQUAL, 6) | borderStyled(BorderStyle::LIGHT) | color(get_rainbow_color(315)),
                               separatorDouble() | color(get_rainbow_color(330)),
                               hbox({
                                   statusPill | flex,
                                   separatorLight(),
                                   actions->Render() | center,
                               }),
                           }) |
                           borderStyled(BorderStyle::DOUBLE) |
                           color(get_rainbow_color(0)) |
                           size(WIDTH, EQUAL, 110) |
                           clear_under;

            auto interactiveLayer = interactiveOverlay->Render();

            if (unattendedManager.IsActive()) {
                auto unattendedUI = unattendedManager.RenderUI(header, get_rainbow_color);
                
                if (showNotification) {
                    return dbox({ unattendedUI, vbox({ notificationLayer, filler() }), interactiveLayer }) | clear_under | center;
                }
                return dbox({ unattendedUI, interactiveLayer }) | clear_under | center;
            }

            if (showNotification) {
                return dbox({
                    mainApp,
                    vbox({ notificationLayer, filler() }),
                    interactiveLayer
                }) | clear_under | center;
            }
            
            return dbox({
                mainApp,
                interactiveLayer
            }) | clear_under | center;
        });

        auto withEvents = CatchEvent(component, [&](Event event) {
            if (event.is_mouse() && std::chrono::steady_clock::now() < ignoreMouseEventsUntil) {
                return true;
            }

            if (event == Event::Escape) {
                // CancelAction overrides the escape key too.
                if (!menuFrame.Menu.CancelAction.empty() && (menuFrame.Menu.CancelAction == "menu.back" || menuExists(menuFrame.Menu.CancelAction))) {
                    navigateToMenuId = menuFrame.Menu.CancelAction;
                } else if (!MenuHistory_.empty()) {
                    requestBack = true;
                } else {
                    shouldQuit = true;
                }
                exitCurrentMenu();
                return true;
            }

            return false;
        });

        screen.Loop(withEvents);

        if (shouldQuit) {
            break;
        }

        if (!navigateToMenuId.empty()) {
            if (navigateToMenuId == "menu.back") {
                if (!MenuHistory_.empty()) {
                    activeMenuId = MenuHistory_.back();
                    MenuHistory_.pop_back();
                    navigateToMenuId.clear();
                } else {
                    activeMenuId = startMenuId;
                    navigateToMenuId.clear();
                }
            } else {
                if (activeMenuId != navigateToMenuId) {
                    MenuHistory_.push_back(activeMenuId);
                }
                activeMenuId = navigateToMenuId;
                navigateToMenuId.clear();
            }
            continue;
        }

        if (requestBack) {
            requestBack = false;
            if (!MenuHistory_.empty()) {
                activeMenuId = MenuHistory_.back();
                MenuHistory_.pop_back();
                continue;
            }

            shouldQuit = true;
            continue;
        }

        if (requestRebuild) {
            continue;
        }

        // Loop exited without explicit navigation/back/rebuild; treat as quit.
        shouldQuit = true;
    }

    // Explicitly disable mouse tracking and restore terminal state.
    // \033[?1000l: Disable X11 mouse tracking
    // \033[?1003l: Disable All motion mouse tracking
    // \033[?1015l: Disable Urxvt mouse tracking
    // \033[?1006l: Disable SGR mouse tracking
    // \033[?25h: Show cursor
    // \033[?47l: Restore normal screen buffer
    printf("\033[?1000l\033[?1003l\033[?1015l\033[?1006l\033[?25h\033[?47l");
    printf("\033[2J\033[H");
    std::fflush(stdout);

    return true;
}

Core::CanvasStatusPillSnapshot CanvasCoreModule::GetCanvasStatusPill() const {
    PumpSignalNotifications();

    Core::CanvasStatusPillSnapshot status;
    status.ModeLabel = "UNKNOWN";
    status.ConnectedInstanceCount = 0;
    status.ProviderId = "none";
    status.Summary = "No instance connectivity provider is active.";

    std::string providerId;
    const auto* provider = ResolveConnectivityProvider(providerId);
    if (!provider) {
        return status;
    }

    try {
        Core::NovaInstanceConnectivitySnapshot snapshot = provider->GetInstanceConnectivitySnapshot();
        if (!providerId.empty() && snapshot.ProviderId.empty()) {
            snapshot.ProviderId = providerId;
        }

        status.ModeLabel = ToCanvasModeLabel(snapshot.Role);
        status.ConnectedInstanceCount = std::max(0, snapshot.ConnectedInstanceCount);
        status.ProviderId = snapshot.ProviderId.empty() ? providerId : snapshot.ProviderId;
        if (status.ProviderId.empty()) {
            status.ProviderId = "unknown";
        }

        if (snapshot.Summary.empty()) {
            status.Summary = status.ModeLabel + " mode with " +
                             std::to_string(status.ConnectedInstanceCount) +
                             " connected instance(s).";
        } else {
            status.Summary = snapshot.Summary;
        }
    } catch (const std::exception& ex) {
        NOVA_LOG((std::string("[CanvasCore] Connectivity snapshot failed: ") + ex.what()).c_str(), LogType::Warning);
        status.ProviderId = providerId.empty() ? "unknown" : providerId;
        status.Summary = "Connectivity provider failed to report a snapshot.";
    } catch (...) {
        NOVA_LOG("[CanvasCore] Connectivity snapshot failed due to unknown exception", LogType::Warning);
        status.ProviderId = providerId.empty() ? "unknown" : providerId;
        status.Summary = "Connectivity provider failed to report a snapshot.";
    }

    return status;
}

std::vector<Core::CanvasToastNotification> CanvasCoreModule::ConsumeCanvasToastsForMenu(
    const std::string& menuId,
    const std::size_t maxCount) const {
    PumpSignalNotifications();

    std::vector<Core::CanvasToastNotification> out;
    std::lock_guard<std::mutex> lock(UiStateMutex_);

    std::size_t allowed = maxCount;
    if (allowed == 0 || allowed > ToastQueue_.size()) {
        allowed = ToastQueue_.size();
    }

    auto it = ToastQueue_.begin();
    while (it != ToastQueue_.end() && out.size() < allowed) {
        if (!MenuIdMatches(it->TargetMenuId, menuId)) {
            ++it;
            continue;
        }

        out.push_back(std::move(*it));
        it = ToastQueue_.erase(it);
    }

    return out;
}

std::vector<Core::CanvasToastNotification> CanvasCoreModule::ConsumeCanvasToasts(const std::size_t maxCount) const {
    return ConsumeCanvasToastsForMenu("", maxCount);
}

std::vector<Core::CanvasPersistentInfoWidget> CanvasCoreModule::GetCanvasPersistentInfos(const std::string& menuId) const {
    PumpSignalNotifications();

    std::vector<Core::CanvasPersistentInfoWidget> out;
    std::lock_guard<std::mutex> lock(UiStateMutex_);

    auto appendEntries = [&](const std::string& key) {
        const auto match = PersistentInfosByMenu_.find(key);
        if (match == PersistentInfosByMenu_.end()) {
            return;
        }

        out.insert(out.end(), match->second.begin(), match->second.end());
    };

    if (menuId.empty()) {
        for (const auto& entry : PersistentInfosByMenu_) {
            out.insert(out.end(), entry.second.begin(), entry.second.end());
        }
        return out;
    }

    const std::string normalizedMenuId = NormalizeMenuId(menuId);
    if (normalizedMenuId != kGlobalCanvasMenuId) {
        appendEntries(kGlobalCanvasMenuId);
    }
    appendEntries(normalizedMenuId);

    return out;
}

void CanvasCoreModule::PublishCanvasToast(const Core::CanvasToastNotification& toast) {
    QueueToast(toast);
}

void CanvasCoreModule::PumpSignalNotifications() const {
    const auto* signalBus = ResolveSignalNotificationBus();
    if (!signalBus) {
        return;
    }

    std::uint64_t afterSequence = 0;
    {
        std::lock_guard<std::mutex> lock(UiStateMutex_);
        afterSequence = LastObservedSignalSequence_;
    }

    std::uint64_t latestSequence = afterSequence;
    const auto notifications = signalBus->ConsumeSignalNotifications(afterSequence, 32, latestSequence);
    if (notifications.empty() && latestSequence <= afterSequence) {
        return;
    }

    std::lock_guard<std::mutex> lock(UiStateMutex_);
    if (latestSequence > LastObservedSignalSequence_) {
        LastObservedSignalSequence_ = latestSequence;
    }

    for (const auto& envelope : notifications) {
        const auto& signal = envelope.Notification;
        const bool routeToToast = signal.Channel.empty() ||
                                  signal.Channel == "canvas.toast" ||
                                  signal.Channel == "canvas.menu.issue";
        const bool routeToPersistentInfo = signal.Persistent || signal.Channel == "canvas.menu.issue";

        if (signal.Channel == Core::SignalChannels::UiControl && signal.Title == Core::SignalTitles::ForceRefresh) {
            if (RedrawCallback_) {
                RedrawCallback_();
            }
            continue;
        }

        if (routeToPersistentInfo) {
            const std::string menuId = NormalizeMenuId(signal.TargetMenuId);
            const std::string code = signal.Code.empty() ? "SignalNotification" : signal.Code;
            const std::string source = signal.SourceExtensionId.empty() ? "signalcore" : signal.SourceExtensionId;
            const std::string message = signal.Message.empty() ? signal.Title : signal.Message;

            if (!message.empty()) {
                const std::string dedupKey = BuildPersistentInfoDedupKey(
                    menuId,
                    signal.TargetFieldId,
                    code,
                    message,
                    source);

                if (PersistentInfoKeys_.insert(dedupKey).second) {
                    auto& bucket = PersistentInfosByMenu_[menuId];

                    Core::CanvasPersistentInfoWidget widget;
                    widget.Id = "canvas-info-" + std::to_string(++RuntimeNotificationCounter_);
                    widget.MenuId = menuId;
                    widget.FieldId = signal.TargetFieldId;
                    widget.Code = code;
                    widget.Source = source;
                    widget.Message = message;
                    widget.CreatedAtUtc = signal.CreatedAtUtc.empty() ? NowUtcIso8601() : signal.CreatedAtUtc;
                    bucket.push_back(std::move(widget));

                    while (bucket.size() > kMaxPersistentInfosPerMenu) {
                        const auto dropped = bucket.front();
                        PersistentInfoKeys_.erase(BuildPersistentInfoDedupKey(
                            dropped.MenuId,
                            dropped.FieldId,
                            dropped.Code,
                            dropped.Message,
                            dropped.Source));
                        bucket.erase(bucket.begin());
                    }
                }
            }
        }

        if (!routeToToast) {
            continue;
        }

        Core::CanvasToastNotification toast;
        toast.Id = "signal-" + std::to_string(envelope.Sequence);
        toast.SourceExtensionId = signal.SourceExtensionId.empty() ? "signalcore" : signal.SourceExtensionId;
        toast.SourceInstanceId = signal.SourceInstanceId;
        toast.TargetMenuId = signal.TargetMenuId;
        toast.TargetFieldId = signal.TargetFieldId;
        toast.Title = signal.Title.empty() ? "Signal" : signal.Title;
        toast.Message = signal.Message.empty() ? signal.Code : signal.Message;
        toast.Severity = ToCanvasSeverity(signal.Severity);
        toast.CreatedAtUtc = signal.CreatedAtUtc.empty() ? NowUtcIso8601() : signal.CreatedAtUtc;
        toast.DisplayDurationMs = signal.Persistent ? 7000 : 4200;

        if (!toast.Message.empty()) {
            QueueToastLocked(std::move(toast));
        }
    }
}

void CanvasCoreModule::QueueToastLocked(Core::CanvasToastNotification toast) const {
    if (toast.Id.empty()) {
        toast.Id = "canvas-toast-" + std::to_string(++RuntimeNotificationCounter_);
    }

    if (toast.CreatedAtUtc.empty()) {
        toast.CreatedAtUtc = NowUtcIso8601();
    }

    if (toast.DisplayDurationMs <= 0) {
        toast.DisplayDurationMs = 4200;
    }

    ToastQueue_.push_back(std::move(toast));
    while (ToastQueue_.size() > kMaxCanvasToastQueueSize) {
        ToastQueue_.pop_front();
    }
}

void CanvasCoreModule::QueueToast(Core::CanvasToastNotification toast) const {
    std::lock_guard<std::mutex> lock(UiStateMutex_);
    QueueToastLocked(std::move(toast));
}

void CanvasCoreModule::RecordPersistentInfo(const std::string& menuId,
                                            const std::string& fieldId,
                                            const std::string& code,
                                            const std::string& message,
                                            const std::string& source) const {
    if (message.empty()) {
        return;
    }

    const std::string normalizedMenuId = NormalizeMenuId(menuId);
    const std::string normalizedCode = code.empty() ? "CanvasRuntimeInfo" : code;
    const std::string normalizedSource = source.empty() ? "canvascore" : source;

    const std::string dedupKey = BuildPersistentInfoDedupKey(
        normalizedMenuId,
        fieldId,
        normalizedCode,
        message,
        normalizedSource);

    std::lock_guard<std::mutex> lock(UiStateMutex_);
    if (!PersistentInfoKeys_.insert(dedupKey).second) {
        return;
    }

    auto& bucket = PersistentInfosByMenu_[normalizedMenuId];

    Core::CanvasPersistentInfoWidget info;
    info.Id = "canvas-info-" + std::to_string(++RuntimeNotificationCounter_);
    info.MenuId = normalizedMenuId;
    info.FieldId = fieldId;
    info.Code = normalizedCode;
    info.Source = normalizedSource;
    info.Message = message;
    info.CreatedAtUtc = NowUtcIso8601();
    bucket.push_back(std::move(info));

    while (bucket.size() > kMaxPersistentInfosPerMenu) {
        const auto dropped = bucket.front();
        PersistentInfoKeys_.erase(BuildPersistentInfoDedupKey(
            dropped.MenuId,
            dropped.FieldId,
            dropped.Code,
            dropped.Message,
            dropped.Source));
        bucket.erase(bucket.begin());
    }
}

void CanvasCoreModule::ClearPersistentInfosForField(const std::string& menuId, const std::string& fieldId) const {
    const std::string normalizedMenuId = NormalizeMenuId(menuId);

    std::lock_guard<std::mutex> lock(UiStateMutex_);
    const auto match = PersistentInfosByMenu_.find(normalizedMenuId);
    if (match == PersistentInfosByMenu_.end()) {
        return;
    }

    auto& bucket = match->second;
    auto iter = bucket.begin();
    while (iter != bucket.end()) {
        const bool remove = fieldId.empty() || iter->FieldId == fieldId;
        if (!remove) {
            ++iter;
            continue;
        }

        PersistentInfoKeys_.erase(BuildPersistentInfoDedupKey(
            iter->MenuId,
            iter->FieldId,
            iter->Code,
            iter->Message,
            iter->Source));
        iter = bucket.erase(iter);
    }

    if (bucket.empty()) {
        PersistentInfosByMenu_.erase(match);
    }
}

void CanvasCoreModule::ClearPersistentInfosForMenu(const std::string& menuId) const {
    const std::string normalizedMenuId = NormalizeMenuId(menuId);

    std::lock_guard<std::mutex> lock(UiStateMutex_);
    const auto match = PersistentInfosByMenu_.find(normalizedMenuId);
    if (match == PersistentInfosByMenu_.end()) {
        return;
    }

    for (const auto& entry : match->second) {
        PersistentInfoKeys_.erase(BuildPersistentInfoDedupKey(
            entry.MenuId,
            entry.FieldId,
            entry.Code,
            entry.Message,
            entry.Source));
    }

    PersistentInfosByMenu_.erase(match);
}

#undef CANVASCORE_CABI_EXPORT
