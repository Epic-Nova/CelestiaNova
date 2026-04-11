#include "Core/RequirementResolver.h"

#include "Core/NovaLog.h"
#include "Core/ExtensionRegistry.h"

#include <algorithm>
#include <sstream>
#include <vector>

namespace Core::RequirementResolver {

namespace {

struct AuthoritativeProviderRule {
    std::string ProviderExtensionId;
    std::string SourceRequirementKey;
    std::vector<std::string> AllowedRequestors;
};

const nlohmann::json* TryGetAuthoritativeProviderRules(const nlohmann::json& descriptor) {
    const auto canvasIt = descriptor.find("canvas");
    if (canvasIt == descriptor.end() || !canvasIt->is_object()) {
        return nullptr;
    }

    const auto requirementsIt = canvasIt->find("requirements");
    if (requirementsIt == canvasIt->end() || !requirementsIt->is_object()) {
        return nullptr;
    }

    const auto resolverIt = requirementsIt->find("resolver");
    if (resolverIt == requirementsIt->end() || !resolverIt->is_object()) {
        return nullptr;
    }

    const auto strategyIt = resolverIt->find("descriptorResolutionStrategy");
    if (strategyIt == resolverIt->end() || !strategyIt->is_object()) {
        return nullptr;
    }

    const auto authoritativeIt = strategyIt->find("authoritativeRequirementSources");
    if (authoritativeIt == strategyIt->end() || !authoritativeIt->is_object()) {
        return nullptr;
    }

    return &(*authoritativeIt);
}

bool ParseAllowedRequestors(const nlohmann::json& ruleValue,
                           std::vector<std::string>& allowedRequestors) {
    const auto allowedIt = ruleValue.find("allowedRequestors");
    if (allowedIt == ruleValue.end()) {
        return true;
    }

    if (!allowedIt->is_array()) {
        return false;
    }

    for (const auto& item : *allowedIt) {
        if (!item.is_string()) {
            continue;
        }

        const std::string requestorId = item.get<std::string>();
        if (!requestorId.empty()) {
            allowedRequestors.push_back(requestorId);
        }
    }

    return true;
}

bool TryBuildAuthoritativeProviderRule(const std::string& providerExtensionId,
                                       const std::string& requirementKey,
                                       const nlohmann::json& ruleValue,
                                       AuthoritativeProviderRule& outRule) {
    outRule.ProviderExtensionId = providerExtensionId;
    outRule.SourceRequirementKey = requirementKey;
    outRule.AllowedRequestors.clear();

    if (ruleValue.is_boolean()) {
        if (!ruleValue.get<bool>()) {
            return false;
        }

        return true;
    }

    if (ruleValue.is_string()) {
        outRule.SourceRequirementKey = ruleValue.get<std::string>();
        return !outRule.SourceRequirementKey.empty();
    }

    if (!ruleValue.is_object()) {
        return false;
    }

    const std::string sourceRequirementKey = ruleValue.value("sourceRequirementKey", requirementKey);
    if (sourceRequirementKey.empty()) {
        return false;
    }
    outRule.SourceRequirementKey = sourceRequirementKey;

    if (!ParseAllowedRequestors(ruleValue, outRule.AllowedRequestors)) {
        return false;
    }

    const std::string explicitProviderId = ruleValue.value("extensionId", "");
    if (!explicitProviderId.empty() && explicitProviderId != providerExtensionId) {
        return false;
    }

    return true;
}

bool IsRequestorAllowed(const std::vector<std::string>& allowedRequestors,
                       const std::string& callerExtensionId) {
    if (allowedRequestors.empty()) {
        return true;
    }

    if (std::find(allowedRequestors.begin(), allowedRequestors.end(), "*") != allowedRequestors.end()) {
        return true;
    }

    if (callerExtensionId.empty()) {
        return false;
    }

    return std::find(allowedRequestors.begin(), allowedRequestors.end(), callerExtensionId) != allowedRequestors.end();
}

bool TryDiscoverAuthoritativeProviderRule(const std::string& requirementKey,
                                          AuthoritativeProviderRule& outRule,
                                          std::string& outErrorCode,
                                          std::string& outErrorMessage) {
    outErrorCode.clear();
    outErrorMessage.clear();

    const auto descriptors = Core::ExtensionRegistry::Instance().ListExtensionDescriptors();
    bool foundRule = false;

    for (const auto& descriptor : descriptors) {
        if (descriptor.id.empty()) {
            continue;
        }

        const auto providerDescriptor = Core::ExtensionDescriptorJson::LoadDescriptorJsonById(descriptor.id);
        if (!providerDescriptor.is_object()) {
            continue;
        }

        const nlohmann::json* rules = TryGetAuthoritativeProviderRules(providerDescriptor);
        if (!rules) {
            continue;
        }

        const auto ruleIt = rules->find(requirementKey);
        if (ruleIt == rules->end()) {
            continue;
        }

        AuthoritativeProviderRule parsedRule;
        if (!TryBuildAuthoritativeProviderRule(descriptor.id, requirementKey, *ruleIt, parsedRule)) {
            continue;
        }

        if (!foundRule) {
            outRule = std::move(parsedRule);
            foundRule = true;
            continue;
        }

        if (outRule.ProviderExtensionId != parsedRule.ProviderExtensionId ||
            outRule.SourceRequirementKey != parsedRule.SourceRequirementKey) {
            outErrorCode = "AmbiguousAuthoritativeProvider";

            std::ostringstream msg;
            msg << "Requirement '" << requirementKey
                << "' is claimed by multiple authoritative providers."
                << " Existing provider='" << outRule.ProviderExtensionId
                << "', conflicting provider='" << parsedRule.ProviderExtensionId << "'.";
            outErrorMessage = msg.str();
            return false;
        }

        for (const std::string& requestorId : parsedRule.AllowedRequestors) {
            if (std::find(outRule.AllowedRequestors.begin(), outRule.AllowedRequestors.end(), requestorId) ==
                outRule.AllowedRequestors.end()) {
                outRule.AllowedRequestors.push_back(requestorId);
            }
        }
    }

    return foundRule;
}

} // namespace

CoreRequirementResolveResult ResolveFromDescriptorDefaults(
    const CoreRequirementResolveRequest& request,
    const std::string& ownerExtensionId,
    const std::string& expectedRequirementKey,
    const std::string& optionsCollectionKey) {
    CoreRequirementResolveResult result;

    if (ownerExtensionId.empty() || expectedRequirementKey.empty() || optionsCollectionKey.empty()) {
        result.Success = false;
        result.ErrorCode = "InvalidResolverConfiguration";
        result.ErrorMessage = "Resolver owner id, requirement key, and options collection key must be configured.";
        return result;
    }

    if (request.RequirementKey != expectedRequirementKey) {
        result.Success = false;
        result.ErrorCode = "UnsupportedRequirement";
        result.ErrorMessage = "Resolver does not support the requested requirement key.";
        return result;
    }

    const auto selfDescriptor = Core::ExtensionDescriptorJson::LoadDescriptorJsonById(ownerExtensionId);
    if (!selfDescriptor.is_object()) {
        result.Success = false;
        result.ErrorCode = "DescriptorNotFound";
        result.ErrorMessage = "Resolver owner descriptor could not be loaded from PluginRegistry.";
        return result;
    }

    const auto preferredResolverIds = Core::ExtensionDescriptorJson::ReadPreferredOtherResolverIds(selfDescriptor);
    for (const std::string& resolverId : preferredResolverIds) {
        const auto otherDescriptor = Core::ExtensionDescriptorJson::LoadDescriptorJsonById(resolverId);
        if (otherDescriptor.is_object()) {
            std::ostringstream msg;
            msg << "[RequirementResolver] Resolved descriptor for extension '" << resolverId << "'";
            NOVA_LOG(msg.str().c_str(), LogType::Log);
        }
    }

    auto optionsDescriptor = selfDescriptor;
    std::string optionsDescriptorId = ownerExtensionId;
    std::string optionsRequirementKey = expectedRequirementKey;

    AuthoritativeProviderRule authoritativeRule;
    std::string authoritativeDiscoveryErrorCode;
    std::string authoritativeDiscoveryErrorMessage;
    const bool hasAuthoritativeRule = TryDiscoverAuthoritativeProviderRule(
        expectedRequirementKey,
        authoritativeRule,
        authoritativeDiscoveryErrorCode,
        authoritativeDiscoveryErrorMessage);

    if (!authoritativeDiscoveryErrorCode.empty()) {
        result.Success = false;
        result.ErrorCode = authoritativeDiscoveryErrorCode;
        result.ErrorMessage = authoritativeDiscoveryErrorMessage;
        return result;
    }

    if (hasAuthoritativeRule) {
        if (!IsRequestorAllowed(authoritativeRule.AllowedRequestors, request.CallerExtensionId)) {
            result.Success = false;
            result.ErrorCode = "RequestorNotAllowed";

            std::ostringstream msg;
            msg << "Caller extension '" << request.CallerExtensionId
                << "' is not permitted to resolve requirement '" << expectedRequirementKey
                << "' from authoritative provider '" << authoritativeRule.ProviderExtensionId << "'.";
            result.ErrorMessage = msg.str();
            return result;
        }

        if (!request.SelectedProviderExtensionId.empty() &&
            request.SelectedProviderExtensionId != authoritativeRule.ProviderExtensionId) {
            result.Success = false;
            result.ErrorCode = "ProviderSelectionMismatch";

            std::ostringstream msg;
            msg << "Requirement '" << expectedRequirementKey << "' must be resolved by provider '"
                << authoritativeRule.ProviderExtensionId << "'.";
            result.ErrorMessage = msg.str();
            return result;
        }

        const auto authoritativeDescriptor = Core::ExtensionDescriptorJson::LoadDescriptorJsonById(
            authoritativeRule.ProviderExtensionId);
        if (!authoritativeDescriptor.is_object()) {
            result.Success = false;
            result.ErrorCode = "AuthoritativeProviderNotFound";

            std::ostringstream msg;
            msg << "Requirement '" << expectedRequirementKey << "' is configured to resolve from provider '"
                << authoritativeRule.ProviderExtensionId << "', but its descriptor could not be loaded.";
            result.ErrorMessage = msg.str();
            return result;
        }

        optionsDescriptor = authoritativeDescriptor;
        optionsDescriptorId = authoritativeRule.ProviderExtensionId;
        optionsRequirementKey = authoritativeRule.SourceRequirementKey;

        std::ostringstream msg;
        msg << "[RequirementResolver] Using authoritative provider descriptor '" << optionsDescriptorId
            << "' for requirement '" << expectedRequirementKey << "' (source key '" << optionsRequirementKey << "').";
        NOVA_LOG(msg.str().c_str(), LogType::Log);
    } else if (!request.SelectedProviderExtensionId.empty() && request.SelectedProviderExtensionId != ownerExtensionId) {
        const auto selectedDescriptor = Core::ExtensionDescriptorJson::LoadDescriptorJsonById(
            request.SelectedProviderExtensionId);
        if (selectedDescriptor.is_object()) {
            optionsDescriptor = selectedDescriptor;
            optionsDescriptorId = request.SelectedProviderExtensionId;

            std::ostringstream msg;
            msg << "[RequirementResolver] Using selected provider descriptor '" << optionsDescriptorId << "'";
            NOVA_LOG(msg.str().c_str(), LogType::Log);
        } else {
            std::ostringstream msg;
            msg << "[RequirementResolver] Selected provider descriptor '" << request.SelectedProviderExtensionId
                << "' was not found. Falling back to owner descriptor '" << ownerExtensionId << "'";
            NOVA_LOG(msg.str().c_str(), LogType::Warning);
        }
    }

    result.Options = Core::ExtensionDescriptorJson::ReadDefaultResolverOptions(
        optionsDescriptor,
        optionsRequirementKey,
        optionsCollectionKey);

    if (result.Options.empty()) {
        result.Success = false;
        result.ErrorCode = "NoOptions";
        result.ErrorMessage = "No resolver options were found in descriptor defaultResponses.";
        return result;
    }

    result.Success = true;
    return result;
}

} // namespace Core::RequirementResolver
