#include "CanvasCore.h"

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
#include "ExtensionSpecific/IInstanceConnectivityProvider.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"
#include "ExtensionSpecific/IOrchestrationSurfaces.h"
#include "ExtensionSpecific/ISignalCoreSurfaces.h"
#include "MenuSchema/CanvasMenuRuntime.h"

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
    std::vector<CanvasCoreResolveOption> options;
    std::set<std::string> seen;

    for (const auto& descriptor : CollectDescriptorSnapshots()) {
        if (!MatchesScope(descriptor, scope)) {
            continue;
        }
        if (IsExcludedProvider(descriptor.Descriptor.id, scope)) {
            continue;
        }

        AddUniqueOption(
            options,
            seen,
            ResolveDisplayName(descriptor),
            descriptor.Descriptor.id,
            descriptor.Descriptor.description);
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
    outFrame.Chrome.Toasts = ConsumeCanvasToastsForMenu(menuId, maxToasts);
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
    std::vector<std::string> menuHistory;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> menuValuesByMenu;

    using namespace ftxui;
    auto screen = ScreenInteractive::FitComponent();

    auto menuExists = [&](const std::string& menuId) {
        CanvasCore::MenuSchema::FCanvasMenuDefinition dummy;
        return GetMenuDefinition(menuId, dummy);
    };

    // Load the mouse-party-mode config flag (written by OptionsMenu to Config/app_config.json). //@TODO: just define through menu json file & set the config files automatically without hardocing any available options, jsut receiving & applying should be in code, not the deifnition , interaction & saving
    bool disableMousePartyMode = false;
    try {
        namespace fs = std::filesystem;
        const fs::path configPath = fs::current_path() / "Config" / "app_config.json";
        if (fs::exists(configPath)) {
            std::ifstream configFile(configPath);
            nlohmann::json configJson;
            configFile >> configJson;
            disableMousePartyMode = configJson.value("disableMousePartyMode", false);
        }
    } catch (...) {}

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
                if (!field.DefaultValue.empty() && menuValues.find(field.Id) == menuValues.end()) {
                    menuValues[field.Id] = field.DefaultValue;
                }
            }
        }

        std::string navigateToMenuId;
        bool requestBack = false;
        bool requestRebuild = false;

        struct FActiveToastState {
            Core::CanvasToastNotification Toast;
            std::chrono::steady_clock::time_point ReceivedAt;
        };

        std::vector<FActiveToastState> activeToasts;
        const auto loopStart = std::chrono::steady_clock::now();
        for (const auto& toast : menuFrame.Chrome.Toasts) {
            activeToasts.push_back({toast, loopStart});
        }

        Core::CanvasStatusPillSnapshot status = menuFrame.Chrome.StatusPill;
        std::vector<Core::CanvasPersistentInfoWidget> persistentInfos = menuFrame.Chrome.PersistentInfos;

        std::vector<std::function<void()>> syncValueCallbacks;

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
                                   const std::function<void()>& callback,
                                   Color bg,
                                   Color fg,
                                   Color bgActive,
                                   Color fgActive) {
            auto option = ButtonOption::Animated(bg, fg, bgActive, fgActive);
            option.transform = [](const EntryState& state) {
                const std::string prefix = state.focused ? "[>" : "[ ";
                const std::string suffix = state.focused ? "<]" : " ]";
                return hbox({
                           text(prefix),
                           text(state.label) | bold,
                           text(suffix),
                       }) |
                       center;
            };
            return Button(label, callback, option);
        };

        Components contentComponents;

        contentComponents.push_back(Renderer([title = menuFrame.Menu.Title, subtitle = menuFrame.Menu.Subtitle] {
            Elements rows;
            rows.push_back(text("== CELESTIA NOVA / CANVASCORE ==") | color(Color::Magenta) | bold | center);
            if (!title.empty()) {
                rows.push_back(text(title) | color(Color::Cyan) | bold | center);
            }
            if (!subtitle.empty()) {
                rows.push_back(paragraph(subtitle) | color(Color::GrayLight) | center);
            }
            rows.push_back(separatorDouble());
            return vbox(std::move(rows));
        }));

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
                if (!field.VisibleIfField.empty()) {
                    std::string visibleValue;
                    const auto visibleMatch = menuValues.find(field.VisibleIfField);
                    if (visibleMatch != menuValues.end()) {
                        visibleValue = visibleMatch->second;
                    }
                    if (visibleValue != field.VisibleIfEquals) {
                        continue;
                    }
                }

                if (field.Type != CanvasCore::MenuSchema::ECanvasFieldType::Separator &&
                    field.Type != CanvasCore::MenuSchema::ECanvasFieldType::Spacer) {
                    contentComponents.push_back(Renderer([label = field.Label, description = field.Description] {
                        Elements rows;
                        rows.push_back(text(label.empty() ? std::string("Field") : label) | color(Color::White) | bold);
                        if (!description.empty()) {
                            rows.push_back(paragraph(description) | color(Color::GrayDark));
                        }
                        return vbox(std::move(rows));
                    }));
                }

                using CanvasCore::MenuSchema::ECanvasFieldType;
                using CanvasCore::MenuSchema::ECanvasValidationKind;

                switch (field.Type) {
                    case ECanvasFieldType::ActionButton: {
                        std::string actionTarget = field.DefaultValue;
                        contentComponents.push_back(makePixelButton(
                            field.Label.empty() ? std::string("Run Action") : field.Label,
                            [&, actionTarget, fieldId = field.Id]() {
                                menuValues[fieldId] = actionTarget;

                                if (!actionTarget.empty() && menuExists(actionTarget)) {
                                    navigateToMenuId = actionTarget;
                                }

                                requestRebuild = true;
                                screen.ExitLoopClosure()();
                            },
                            Color::Black,
                            Color::Cyan,
                            Color::Cyan,
                            Color::Black));
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
                        if (selectedValue != menuValues.end()) {
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
                        const std::string line = field.DefaultValue.empty() ? field.Label : field.DefaultValue;
                        contentComponents.push_back(Renderer([line] { return text(line) | color(Color::White); }));
                        break;
                    }

                    case ECanvasFieldType::Paragraph: {
                        const std::string paragraphText = field.DefaultValue.empty() ? field.Description : field.DefaultValue;
                        contentComponents.push_back(Renderer([paragraphText] { return paragraph(paragraphText) | color(Color::GrayLight); }));
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
                        std::string sourceValue = field.DefaultValue;
                        const auto valueIt = menuValues.find(field.Id);
                        if (valueIt != menuValues.end() && !valueIt->second.empty()) {
                            sourceValue = valueIt->second;
                        }

                        double progressValue = ParseDoubleOrDefault(sourceValue, 0.0);
                        if (progressValue > 1.0) {
                            progressValue /= 100.0;
                        }
                        const float progress = Clamp01(progressValue);

                        contentComponents.push_back(Renderer([
                            progress,
                            label = field.Label,
                            isDirectional = field.Type == ECanvasFieldType::DirectionalGauge,
                            templateName = field.RenderTemplate] {
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
                                text(label.empty() ? std::string("Gauge") : label) | color(Color::White) | bold,
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
            "Apply",
            [&]() {
                auto collectedValues = collectSubmitValues();
                std::string payload;
                std::string buildError;
                if (BuildSubmitPayload(activeMenuId, collectedValues, payload, buildError)) {
                    Core::CanvasToastNotification toast;
                    toast.SourceExtensionId = "canvascore";
                    toast.TargetMenuId = NormalizeMenuId(activeMenuId);
                    toast.Title = "Submit payload built";
                    toast.Message = menuFrame.Menu.SubmitAction.empty()
                        ? "Payload generated from canvas field values."
                        : ("Action '" + menuFrame.Menu.SubmitAction + "' payload generated.");
                    toast.Severity = Core::CanvasNotificationSeverity::Success;
                    PublishCanvasToast(toast);

                    std::string logPayload = "[CanvasCore] submit payload for menu '" + activeMenuId + "':\n" + payload;
                    NOVA_LOG(logPayload.c_str(), LogType::Log);

                    // If SubmitAction names a menu, navigate there on success.
                    if (!menuFrame.Menu.SubmitAction.empty() && menuExists(menuFrame.Menu.SubmitAction)) {
                        navigateToMenuId = menuFrame.Menu.SubmitAction;
                    } else {
                        requestRebuild = true;
                    }
                } else {
                    Core::CanvasToastNotification toast;
                    toast.SourceExtensionId = "canvascore";
                    toast.TargetMenuId = NormalizeMenuId(activeMenuId);
                    toast.Title = "Submit payload failed";
                    toast.Message = buildError;
                    toast.Severity = Core::CanvasNotificationSeverity::Error;
                    PublishCanvasToast(toast);
                    requestRebuild = true;
                }

                screen.ExitLoopClosure()();
            },
            Color::Black,
            Color::Green,
            Color::Green,
            Color::Black);

        auto refreshButton = makePixelButton(
            "Refresh",
            [&]() {
                requestRebuild = true;
                screen.ExitLoopClosure()();
            },
            Color::Black,
            Color::Cyan,
            Color::Cyan,
            Color::Black);

        auto backButton = makePixelButton(
            "Back",
            [&]() {
                // CancelAction in the menu definition overrides the default
                // history-pop behaviour, making navigation fully metadata-driven.
                if (!menuFrame.Menu.CancelAction.empty() && menuExists(menuFrame.Menu.CancelAction)) {
                    navigateToMenuId = menuFrame.Menu.CancelAction;
                } else {
                    requestBack = true;
                }
                screen.ExitLoopClosure()();
            },
            Color::Black,
            Color::Yellow,
            Color::Yellow,
            Color::Black);

        auto quitButton = makePixelButton(
            "Quit",
            [&]() {
                shouldQuit = true;
                screen.ExitLoopClosure()();
            },
            Color::Black,
            Color::Red,
            Color::Red,
            Color::White);

        auto actions = Container::Horizontal({applyButton, refreshButton, backButton, quitButton});
        auto root = Container::Vertical({contentContainer, actions});

        auto component = Renderer(root, [&]() -> Element {
            for (const auto& syncCallback : syncValueCallbacks) {
                syncCallback();
            }

            status = GetCanvasStatusPill();
            persistentInfos = GetCanvasPersistentInfos(activeMenuId);

            const auto now = std::chrono::steady_clock::now();
            const auto newToasts = ConsumeCanvasToastsForMenu(activeMenuId, 3);
            for (const auto& toast : newToasts) {
                activeToasts.push_back({toast, now});
            }

            activeToasts.erase(
                std::remove_if(activeToasts.begin(), activeToasts.end(), [&](const FActiveToastState& toast) {
                    const auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - toast.ReceivedAt).count();
                    const int maxAge = toast.Toast.DisplayDurationMs > 0 ? toast.Toast.DisplayDurationMs : 4200;
                    return ageMs > maxAge;
                }),
                activeToasts.end());

            Color statusColor = Color::Yellow;
            if (status.ModeLabel == "HOST") {
                statusColor = Color::Green;
            } else if (status.ModeLabel == "CLIENT") {
                statusColor = Color::Cyan;
            } else if (status.ModeLabel == "LOCAL") {
                statusColor = Color::White;
            }

            Element statusPill = hbox({
                text("[ "),
                text(status.ModeLabel.empty() ? std::string("UNKNOWN") : status.ModeLabel) | color(statusColor) | bold,
                text(" | PEERS "),
                text(std::to_string(status.ConnectedInstanceCount)) | color(statusColor) | bold,
                text(" ]"),
            }) | center;

            Elements infoRows;
            infoRows.push_back(text("[ Canvas Diagnostics ]") | color(Color::Yellow) | bold);
            if (persistentInfos.empty()) {
                infoRows.push_back(text("OK - no persistent issues") | color(Color::Green));
            } else {
                const std::size_t visibleInfos = persistentInfos.size() > 3 ? 3 : persistentInfos.size();
                for (std::size_t index = 0; index < visibleInfos; ++index) {
                    const auto& info = persistentInfos[persistentInfos.size() - 1 - index];
                    std::string line = info.Code + ": " + info.Message;
                    if (line.size() > 92) {
                        line = line.substr(0, 89) + "...";
                    }
                    infoRows.push_back(text(line) | color(Color::Red));
                }
            }

            Elements toastRows;
            if (!activeToasts.empty()) {
                toastRows.push_back(text("[ Toasts ]") | color(Color::Cyan) | bold);
                const std::size_t visibleToasts = activeToasts.size() > 3 ? 3 : activeToasts.size();
                for (std::size_t index = 0; index < visibleToasts; ++index) {
                    const auto& toastState = activeToasts[activeToasts.size() - 1 - index];

                    std::string severityLabel = "INFO";
                    Color severityColor = Color::Cyan;
                    switch (toastState.Toast.Severity) {
                        case Core::CanvasNotificationSeverity::Success:
                            severityLabel = "OK";
                            severityColor = Color::Green;
                            break;
                        case Core::CanvasNotificationSeverity::Warning:
                            severityLabel = "WARN";
                            severityColor = Color::Yellow;
                            break;
                        case Core::CanvasNotificationSeverity::Error:
                            severityLabel = "ERR";
                            severityColor = Color::Red;
                            break;
                        case Core::CanvasNotificationSeverity::Critical:
                            severityLabel = "CRIT";
                            severityColor = Color::Red;
                            break;
                        case Core::CanvasNotificationSeverity::Info:
                        default:
                            break;
                    }

                    std::string toastMessage = toastState.Toast.Message;
                    if (toastMessage.size() > 84) {
                        toastMessage = toastMessage.substr(0, 81) + "...";
                    }

                    toastRows.push_back(
                        hbox({
                            text("[") | color(severityColor),
                            text(severityLabel) | color(severityColor) | bold,
                            text("] ") | color(severityColor),
                            text(toastState.Toast.Title.empty() ? std::string("Signal") : toastState.Toast.Title) | color(Color::White) | bold,
                            text(" :: "),
                            text(toastMessage) | color(Color::GrayLight),
                        }));
                }
            }

            return vbox({
                       text("== CANVAS RUNTIME ==") | color(Color::Magenta) | bold | center,
                       statusPill,
                       text(status.Summary) | color(Color::GrayLight) | center,
                       separatorDouble(),
                       window(text(menuFrame.Menu.Id), contentContainer->Render() | vscroll_indicator | size(HEIGHT, LESS_THAN, 24)) |
                           color(Color::White),
                       separatorLight(),
                       window(text("Diagnostics"), vbox(std::move(infoRows)) | size(HEIGHT, LESS_THAN, 8)),
                       toastRows.empty() ? text("") : window(text("Notifications"), vbox(std::move(toastRows)) | size(HEIGHT, LESS_THAN, 8)),
                       separatorLight(),
                       actions->Render() | center,
                       text("Esc: Back/Quit | Enter: Activate") | color(Color::GrayDark) | center,
                   }) |
                   size(WIDTH, EQUAL, 110) |
                   center;
        });

        auto withEvents = CatchEvent(component, [&](Event event) {
            // Suppress all mouse events when "Light Party" (mouse party mode) is disabled.
            if (disableMousePartyMode && event.is_mouse()) {
                return true;
            }

            if (event == Event::Escape) {
                // CancelAction overrides the escape key too.
                if (!menuFrame.Menu.CancelAction.empty() && menuExists(menuFrame.Menu.CancelAction)) {
                    navigateToMenuId = menuFrame.Menu.CancelAction;
                } else if (!menuHistory.empty()) {
                    requestBack = true;
                } else {
                    shouldQuit = true;
                }
                screen.ExitLoopClosure()();
                return true;
            }

            return false;
        });

        screen.Loop(withEvents);

        if (shouldQuit) {
            break;
        }

        if (!navigateToMenuId.empty()) {
            menuHistory.push_back(activeMenuId);
            activeMenuId = navigateToMenuId;
            continue;
        }

        if (requestBack) {
            if (!menuHistory.empty()) {
                activeMenuId = menuHistory.back();
                menuHistory.pop_back();
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
