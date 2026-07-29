#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"
#include "ExtensionSpecific/IStatusRoutingPolicyProvider.h"
#include "ExtensionSpecific/IInstanceConnectivityProvider.h"
#include "ExtensionSpecific/IStatusSnapshotProvider.h"

#ifdef NexusCore_EXPORTS
#  define NEXUSCORE_API NOVA_EXPORT
#else
#  define NEXUSCORE_API NOVA_IMPORT
#endif

class NEXUSCORE_API NexusCoreModule : public IExtensionInterface,
                                      public Core::INovaCapabilityProvider,
                                      public Core::IStatusRoutingPolicyProvider,
                                      public Core::IInstanceConnectivityProvider,
                                      public Core::IStatusSnapshotProvider {
public:
    NexusCoreModule();
    ~NexusCoreModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;

    int GetStatusRoutingPolicyPriority() const override;
    bool AcceptsProviderForDomain(Core::StatusDeclarationDomain domain,
                                  const std::string& providerId) const override;

    Core::NovaInstanceConnectivitySnapshot GetInstanceConnectivitySnapshot() const override;
    std::string BuildDaemonStatusJson() const override;

    /**
     * [Scaffolding] Reports the gathered instance state (capabilities, content)
     * to authoritative management instances.
     */
    void ReportToAuthoritativeInstances();

    /**
     * Returns a list of discovered remote Celestia Nova instances.
     */
    std::vector<std::string> GetRemoteInstances() const;

    /**
     * Sends a command to a remote instance.
     */
    bool DispatchRemoteCommand(const std::string& instanceId, const std::string& command);

private:
    std::vector<std::string> AuthoritativeEndpoints_;
};
