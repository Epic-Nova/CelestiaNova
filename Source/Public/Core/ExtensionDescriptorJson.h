#pragma once

#include <string>
#include <vector>

#include "Core/ModuleAPI.h"
#include "json.hpp"

namespace Core::ExtensionDescriptorJson {

struct ResolverOption {
    std::string Label;
    std::string Value;
    std::string Description;
};

// Loads an extension descriptor JSON object by extension id using PluginRegistry.
NOVA_CORE_API nlohmann::json LoadDescriptorJsonById(const std::string& extensionId);

// Reads canvas.requirements.resolver.descriptorResolutionStrategy.others.preferredExtensions.
NOVA_CORE_API std::vector<std::string> ReadPreferredOtherResolverIds(const nlohmann::json& descriptor);

// Reads normalized options from canvas.requirements.resolver.defaultResponses.<requirementKey>.<collectionKey>.
NOVA_CORE_API std::vector<ResolverOption> ReadDefaultResolverOptions(const nlohmann::json& descriptor,
                                                                     const std::string& requirementKey,
                                                                     const std::string& collectionKey);

} // namespace Core::ExtensionDescriptorJson
