#include "Core/ExtensionDescriptorJson.h"

#include "Core/NovaFileOperations.h"
#include "Core/NovaLog.h"
#include "Core/ExtensionRegistry.h"

#include <exception>
#include <utility>

namespace {

const nlohmann::json* TryGetObjectPath(const nlohmann::json& root,
                                       const std::vector<std::string>& pathSegments) {
    const nlohmann::json* current = &root;

    for (const std::string& key : pathSegments) {
        if (!current->is_object()) {
            return nullptr;
        }

        const auto it = current->find(key);
        if (it == current->end()) {
            return nullptr;
        }

        current = &(*it);
    }

    return current;
}

} // namespace

namespace Core::ExtensionDescriptorJson {

nlohmann::json LoadDescriptorJsonById(const std::string& extensionId) {
    auto& registry = Core::ExtensionRegistry::Instance();
    const std::string descriptorPath = registry.GetExtensionDescriptorPath(extensionId);
    if (descriptorPath.empty()) {
        return nlohmann::json();
    }

    const std::string body = Core::FileOperations::NovaFileOperations::ReadTextFile(descriptorPath);
    if (body.empty()) {
        return nlohmann::json();
    }

    try {
        return nlohmann::json::parse(body);
    } catch (const std::exception& ex) {
        NOVA_LOG(("ExtensionDescriptorJson: failed to parse descriptor for '" + extensionId + "': " + ex.what()).c_str(), LogType::Warning);
    } catch (...) {
        NOVA_LOG(("ExtensionDescriptorJson: failed to parse descriptor for '" + extensionId + "' due to unknown exception").c_str(), LogType::Warning);
    }

    return nlohmann::json();
}

std::vector<std::string> ReadPreferredOtherResolverIds(const nlohmann::json& descriptor) {
    static const std::vector<std::string> kPath = {
        "canvas",
        "requirements",
        "resolver",
        "descriptorResolutionStrategy",
        "others",
        "preferredExtensions"
    };

    std::vector<std::string> ids;
    const nlohmann::json* preferred = TryGetObjectPath(descriptor, kPath);
    if (!preferred || !preferred->is_array()) {
        return ids;
    }

    for (const auto& item : *preferred) {
        if (!item.is_string()) {
            continue;
        }

        const std::string id = item.get<std::string>();
        if (!id.empty()) {
            ids.push_back(id);
        }
    }

    return ids;
}

std::vector<ResolverOption> ReadDefaultResolverOptions(const nlohmann::json& descriptor,
                                                       const std::string& requirementKey,
                                                       const std::string& collectionKey) {
    std::vector<ResolverOption> options;
    if (requirementKey.empty() || collectionKey.empty()) {
        return options;
    }

    static const std::vector<std::string> kDefaultsPath = {
        "canvas",
        "requirements",
        "resolver",
        "defaultResponses"
    };

    const nlohmann::json* defaults = TryGetObjectPath(descriptor, kDefaultsPath);
    if (!defaults || !defaults->is_object()) {
        return options;
    }

    const auto requirementIt = defaults->find(requirementKey);
    if (requirementIt == defaults->end() || !requirementIt->is_object()) {
        return options;
    }

    const auto collectionIt = requirementIt->find(collectionKey);
    if (collectionIt == requirementIt->end() || !collectionIt->is_array()) {
        return options;
    }

    for (const auto& item : *collectionIt) {
        if (!item.is_object()) {
            continue;
        }

        ResolverOption option;
        option.Label = item.value("displayName", "");
        option.Value = item.value("name", "");
        option.Description = item.value("description", "");

        if (!option.Label.empty() && !option.Value.empty()) {
            options.push_back(std::move(option));
        }
    }

    return options;
}

} // namespace Core::ExtensionDescriptorJson
