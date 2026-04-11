#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace CanvasCore {
namespace MenuSchema {

// Data contract for CanvasCore's data-driven menu system.

enum class ECanvasFieldType {
    Bool,
    String,
    PasswordString,
    Integer,
    Float,
    ProviderSelect,
    ProviderMultiSelect,
    ToggleGroup,
    RadioGroup,
    Dropdown,
    SliderInt,
    SliderFloat,
    ActionButton,
    TextLabel,
    Paragraph,
    Separator,
    Spacer,
    Table,
    ProgressGauge,
    DirectionalGauge,
    SparklineGraph,
    Spinner,
    CanvasChart,
    StructObject,
    WindowPane,
    CollapsibleSection,
    ResizableSplit,
    TabContainer
};

enum class ECanvasProviderSource {
    AnyExtension,
    Core,
    Agent,
    Service,
    Orchestrator
};

enum class ECanvasRequirementResolveMode {
    ProviderList,
    ValueList,
    StructuredObject
};

enum class ECanvasResolverExecutionStrategy {
    RegistryLookup,
    ApiRequest,
    ContentQuery,
    Custom
};

enum class ECanvasValueKind {
    Bool,
    String,
    Integer,
    Float,
    Object,
    Array
};

enum class ECanvasValueStorage {
    RuntimeOnly,
    ConfigJson,
    Session,
    SecretStore
};

enum class ECanvasValidationKind {
    None,
    Required,
    Min,
    Max,
    Regex,
    ExistingPath,
    ExistingDirectory
};

struct FCanvasFieldValidation {
    ECanvasValidationKind Kind = ECanvasValidationKind::None;
    std::string Argument;
    std::string Message;
};

struct FCanvasResolveContextBinding {
    std::string RequestKey;
    std::string FromFieldId;
    std::string FallbackValue;
};

struct FCanvasRequirementBinding {
    std::string RequirementKey;
    ECanvasProviderSource Source = ECanvasProviderSource::AnyExtension;
    ECanvasRequirementResolveMode ResolveMode = ECanvasRequirementResolveMode::ProviderList;
    ECanvasResolverExecutionStrategy Strategy = ECanvasResolverExecutionStrategy::RegistryLookup;
    bool AllowMultiple = false;

    // Resolver preference only. Any extension may resolve the requirement key.
    std::vector<std::string> PreferredResolverExtensionIds;

    // Optional resolver execution hints for universal API, lookup, and query flows.
    std::string RequestAction;
    std::string ResponseCollectionPath;
    std::string ResponseLabelPath = "label";
    std::string ResponseValuePath = "value";
    std::vector<FCanvasResolveContextBinding> ContextBindings;

    // Optional contract metadata for C++ struct construction.
    std::string RequestStructName;
    std::string OutputStructName;
};

struct FCanvasResolvedOption {
    std::string Label;
    std::string Value;
    std::string Description;
    std::vector<std::pair<std::string, std::string>> Metadata;
};

struct FCanvasRequirementResolveRequest {
    std::string CorrelationId;
    std::string ConsumerExtensionId;
    std::string MenuId;
    std::string FieldId;
    FCanvasRequirementBinding Binding;

    // Snapshot of already-collected form values used by ContextBindings.
    std::vector<std::pair<std::string, std::string>> ContextValues;
};

struct FCanvasRequirementResolveResult {
    bool Success = false;
    std::string ResolverExtensionId;
    std::string ErrorCode;
    std::string ErrorMessage;
    std::vector<FCanvasResolvedOption> Options;
};

struct FCanvasStructFieldDefinition {
    std::string Id;
    std::string Label;
    std::string Description;
    ECanvasValueKind ValueKind = ECanvasValueKind::String;
    bool Required = false;
    std::string DefaultValue;
    std::vector<FCanvasStructFieldDefinition> Children;
};

struct FCanvasFieldDefinition {
    std::string Id;
    std::string Label;
    std::string Description;
    ECanvasFieldType Type = ECanvasFieldType::String;

    // Default for scalar-like fields.
    std::string DefaultValue;

    // Populates dynamic values from requirement resolution.
    std::optional<FCanvasRequirementBinding> RequirementBinding;

    // For StructObject and structured requirement inputs.
    std::vector<FCanvasStructFieldDefinition> StructFields;

    // For stats/visual fields (graph/gauge/table) resolved by requirement resolvers.
    std::string DataRequirementKey;
    std::string RenderTemplate;

    std::vector<FCanvasFieldValidation> Validations;
    ECanvasValueStorage ValueStorage = ECanvasValueStorage::RuntimeOnly;

    // Optional conditional display logic.
    std::string VisibleIfField;
    std::string VisibleIfEquals;

    // Final payload key passed to downstream service/orchestrator.
    std::string OutputKey;
};

struct FCanvasSectionDefinition {
    std::string Id;
    std::string Title;
    std::string Description;
    std::vector<FCanvasFieldDefinition> Fields;
};

struct FCanvasMenuDefinition {
    std::string Id;
    std::string Title;
    std::string Subtitle;
    std::string Icon;

    // Semantic requirements for this menu (for requirement pre-checks).
    std::vector<std::string> PageRequirements;

    std::vector<FCanvasSectionDefinition> Sections;

    std::string SubmitAction;
    std::string CancelAction;
};

struct FCanvasMenuFile {
    std::string Schema = "canvas.menu.definitions.v1";
    std::string OwnerExtensionId;
    std::vector<FCanvasMenuDefinition> Menus;
};

} // namespace MenuSchema
} // namespace CanvasCore
