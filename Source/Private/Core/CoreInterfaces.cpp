#include "Core/IJsonStructParser.h"
#include "Core/IExtensionInterface.h"
#include "ExtensionSpecific/ICanvasRuntimeSurfaceProvider.h"
#include "ExtensionSpecific/ISignalCoreSurfaces.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"
#include "ExtensionSpecific/IInstanceConnectivityProvider.h"
#include "ExtensionSpecific/IOrchestrationSurfaces.h"
#include "ExtensionSpecific/IPersistenceSurfaces.h"
#include "ExtensionSpecific/IStatusRoutingPolicyProvider.h"

namespace Core {

// IJsonSchemaValidator
IJsonSchemaValidator::~IJsonSchemaValidator() = default;

// ICanvasRuntimeSurfaceProvider
ICanvasRuntimeSurfaceProvider::~ICanvasRuntimeSurfaceProvider() = default;

// ISignalNotificationBus
ISignalNotificationBus::~ISignalNotificationBus() = default;

// INovaCapabilityProvider
INovaCapabilityProvider::~INovaCapabilityProvider() = default;

// IInstanceConnectivityProvider
IInstanceConnectivityProvider::~IInstanceConnectivityProvider() = default;

// IOrchestratorSetupProfileProvider
IOrchestratorSetupProfileProvider::~IOrchestratorSetupProfileProvider() = default;

// IOrchestratorInteractionLifecycleProvider
IOrchestratorInteractionLifecycleProvider::~IOrchestratorInteractionLifecycleProvider() = default;

// INovaPersistenceSurface
INovaPersistenceSurface::~INovaPersistenceSurface() = default;

// INovaPersistenceSurfaceProvider
INovaPersistenceSurfaceProvider::~INovaPersistenceSurfaceProvider() = default;

// IStatusRoutingPolicyProvider
IStatusRoutingPolicyProvider::~IStatusRoutingPolicyProvider() = default;

} // namespace Core
