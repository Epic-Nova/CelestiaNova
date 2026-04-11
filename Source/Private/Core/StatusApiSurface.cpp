#include "Core/StatusApiSurface.h"

#include "Core/ExtensionRegistry.h"
#include "Core/NovaLog.h"
#include "ExtensionSpecific/IStatusRoutingPolicyProvider.h"

#include <algorithm>
#include <exception>
#include <limits>
#include <unordered_set>

namespace Core {

namespace {

std::vector<NovaCapabilityDescriptor> CollectCapabilityDescriptors() {
    std::vector<NovaCapabilityDescriptor> descriptors;

    const auto registered = ExtensionRegistry::Instance().ListExtensionDescriptors();
    descriptors.reserve(registered.size());

    for (const auto& descriptor : registered) {
        IExtensionInterface* instance = ExtensionRegistry::Instance().GetLoadedExtensionInstance(descriptor.id);
        if (!instance) {
            continue;
        }

        auto* provider = dynamic_cast<INovaCapabilityProvider*>(instance);
        if (!provider) {
            continue;
        }

        try {
            auto capability = provider->GetCapabilityDescriptor();
            if (capability.providerId.empty()) {
                capability.providerId = descriptor.id;
            }
            if (capability.displayName.empty()) {
                capability.displayName = descriptor.name;
            }
            descriptors.push_back(std::move(capability));
        } catch (const std::exception& ex) {
            NOVA_LOG(("StatusApiSurface: capability descriptor query failed for '" + descriptor.id + "': " + ex.what()).c_str(), LogType::Warning);
        } catch (...) {
            NOVA_LOG(("StatusApiSurface: capability descriptor query failed for '" + descriptor.id + "' due to unknown exception").c_str(), LogType::Warning);
        }
    }

    return descriptors;
}

const IStatusRoutingPolicyProvider* ResolveRoutingPolicyProvider() {
    const auto registered = ExtensionRegistry::Instance().ListExtensionDescriptors();

    const IStatusRoutingPolicyProvider* selected = nullptr;
    int selectedPriority = std::numeric_limits<int>::lowest();

    for (const auto& descriptor : registered) {
        IExtensionInterface* instance = ExtensionRegistry::Instance().GetLoadedExtensionInstance(descriptor.id);
        if (!instance) {
            continue;
        }

        auto* policyProvider = dynamic_cast<IStatusRoutingPolicyProvider*>(instance);
        if (!policyProvider) {
            continue;
        }

        const int candidatePriority = policyProvider->GetStatusRoutingPolicyPriority();
        if (!selected || candidatePriority > selectedPriority) {
            selected = policyProvider;
            selectedPriority = candidatePriority;
        }
    }

    return selected;
}

template <typename FieldSelector>
std::vector<std::string> CollectUniqueCapabilityValues(StatusDeclarationDomain domain, FieldSelector selector) {
    std::unordered_set<std::string> seen;
    std::vector<std::string> out;

    const IStatusRoutingPolicyProvider* routingPolicy = ResolveRoutingPolicyProvider();

    for (const auto& descriptor : CollectCapabilityDescriptors()) {
        if (routingPolicy && !routingPolicy->AcceptsProviderForDomain(domain, descriptor.providerId)) {
            continue;
        }

        for (const auto& value : selector(descriptor)) {
            if (value.empty()) {
                continue;
            }
            if (seen.insert(value).second) {
                out.push_back(value);
            }
        }
    }

    std::sort(out.begin(), out.end());
    return out;
}

} // namespace

std::string StatusApiSurface::BuildExtensionsStatusJson() {
    return ExtensionRegistry::Instance().BuildExtensionStatusSnapshotJson();
}

std::vector<std::string> StatusApiSurface::ListDeclaredHealthEndpoints() {
    return CollectUniqueCapabilityValues(StatusDeclarationDomain::HealthEndpoints,
        [](const NovaCapabilityDescriptor& descriptor) -> const std::vector<std::string>& {
        return descriptor.healthEndpoints;
    });
}

std::vector<std::string> StatusApiSurface::ListDeclaredContentEndpoints() {
    return CollectUniqueCapabilityValues(StatusDeclarationDomain::ContentEndpoints,
        [](const NovaCapabilityDescriptor& descriptor) -> const std::vector<std::string>& {
        return descriptor.contentEndpoints;
    });
}

std::vector<std::string> StatusApiSurface::ListDeclaredGrafanaDashboards() {
    return CollectUniqueCapabilityValues(StatusDeclarationDomain::GrafanaDashboards,
        [](const NovaCapabilityDescriptor& descriptor) -> const std::vector<std::string>& {
        return descriptor.grafanaDashboards;
    });
}

std::vector<std::string> StatusApiSurface::ListDeclaredServiceCapabilities() {
    return CollectUniqueCapabilityValues(StatusDeclarationDomain::ServiceCapabilities,
        [](const NovaCapabilityDescriptor& descriptor) -> const std::vector<std::string>& {
        return descriptor.serviceCapabilities;
    });
}

std::vector<std::string> StatusApiSurface::ListDeclaredContentPacks() {
    return CollectUniqueCapabilityValues(StatusDeclarationDomain::ContentPacks,
        [](const NovaCapabilityDescriptor& descriptor) -> const std::vector<std::string>& {
        return descriptor.contentPacks;
    });
}

std::vector<std::string> StatusApiSurface::ListDeclaredTelemetryStreams() {
    return CollectUniqueCapabilityValues(StatusDeclarationDomain::TelemetryStreams,
        [](const NovaCapabilityDescriptor& descriptor) -> const std::vector<std::string>& {
        return descriptor.telemetryStreams;
    });
}

} // namespace Core
