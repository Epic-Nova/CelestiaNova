#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Core/ExtensionDescriptorJson.h"
#include "Core/ModuleAPI.h"

namespace Core::RequirementResolver {

using CoreRequirementResolvedOption = Core::ExtensionDescriptorJson::ResolverOption;

struct CoreRequirementResolveRequest {
    std::string RequirementKey;
    std::string CallerExtensionId;
    std::string SelectedProviderExtensionId;
    std::vector<std::pair<std::string, std::string>> ContextValues;
};

struct CoreRequirementResolveResult {
    bool Success = false;
    std::string ErrorCode;
    std::string ErrorMessage;
    std::vector<CoreRequirementResolvedOption> Options;
};

// Resolves a requirement from descriptor defaults using the shared canvas resolver shape.
NOVA_CORE_API CoreRequirementResolveResult ResolveFromDescriptorDefaults(
    const CoreRequirementResolveRequest& request,
    const std::string& ownerExtensionId,
    const std::string& expectedRequirementKey,
    const std::string& optionsCollectionKey);

// Dispatch helper for exported C ABI resolver bridges.
template <typename TResolveFn>
bool DispatchResolveRequest(const void* requestPtr, void* resultPtr, TResolveFn&& resolveFn) {
    if (!requestPtr || !resultPtr) {
        return false;
    }

    const auto* typedRequest = static_cast<const CoreRequirementResolveRequest*>(requestPtr);
    auto* typedResult = static_cast<CoreRequirementResolveResult*>(resultPtr);
    *typedResult = std::forward<TResolveFn>(resolveFn)(*typedRequest);
    return typedResult->Success;
}

} // namespace Core::RequirementResolver
