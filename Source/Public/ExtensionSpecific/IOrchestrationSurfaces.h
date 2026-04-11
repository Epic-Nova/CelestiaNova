#pragma once

#include <string>
#include <vector>
#include "Core/ModuleAPI.h"

namespace Core {

enum class SetupProfileDepth {
    Auto,
    Simple,
    Advanced,
    Expert,
};

enum class SetupFieldType {
    Toggle,
    Select,
    MultiSelect,
    InputText,
    Password,
    Number,
    Path,
};

struct SetupSurfaceOption {
    std::string value;
    std::string label;
    std::string description;
    bool recommended = false;
};

struct SetupSurfaceField {
    std::string key;
    std::string label;
    SetupFieldType type = SetupFieldType::InputText;
    bool required = false;
    std::string placeholder;
    std::string defaultValue;
    std::vector<SetupSurfaceOption> options;
};

struct SetupSurfaceSection {
    std::string id;
    std::string title;
    std::string description;
    std::vector<SetupSurfaceField> fields;
};

struct SetupSurfaceRequest {
    std::string extensionId;
    std::string serviceId;
    std::string environment;
    std::string scalingManagerExtensionId;
    std::string scalingStoreRef;
    std::string persistenceSurfaceId;
    std::string persistenceNamespace;
    std::string persistenceCollection;
    bool scalingRequested = false;
    std::vector<SetupSurfaceOption> managedEnvironmentOptions;
    std::vector<std::string> activeOrchestrators;
    std::vector<std::string> activeDatabaseOrchestrators;
};

struct OrchestratorSetupSurface {
    SetupProfileDepth depth = SetupProfileDepth::Simple;
    std::vector<SetupSurfaceSection> sections;
    std::vector<std::string> uiContributors;
};

enum class InteractionLifecycleStage {
    Configuration,
    Migration,
    Interaction,
};

enum class InteractionActionType {
    Validate,
    Preview,
    Apply,
    Start,
    Stop,
    Restart,
    FetchLogs,
    Rollback,
};

struct InteractionLifecycleAction {
    InteractionLifecycleStage stage = InteractionLifecycleStage::Interaction;
    InteractionActionType actionType = InteractionActionType::Apply;
    std::string actionId;
    std::string displayName;
    std::string description;
    bool requiresConfirmation = false;
    bool requiresElevation = false;
};

struct InteractionLifecycleContract {
    std::string providerId;
    std::string summary;
    std::vector<InteractionLifecycleAction> actions;
    std::vector<std::string> emittedEvents;
};

// Implemented by orchestrators that expose setup profile-depth specific UI
// sections and context-driven configuration surfaces.
class NOVA_CORE_API IOrchestratorSetupProfileProvider {
public:
    virtual ~IOrchestratorSetupProfileProvider();

    virtual std::vector<SetupProfileDepth> GetSupportedProfileDepths() const = 0;
    virtual OrchestratorSetupSurface BuildSetupSurface(const SetupSurfaceRequest& request, SetupProfileDepth depth) const = 0;
};

// Implemented by orchestrators that expose lifecycle action contracts
// for configuration, migration, and runtime interaction phases.
class NOVA_CORE_API IOrchestratorInteractionLifecycleProvider {
public:
    virtual ~IOrchestratorInteractionLifecycleProvider();

    virtual InteractionLifecycleContract GetInteractionLifecycleContract() const = 0;
};

} // namespace Core
