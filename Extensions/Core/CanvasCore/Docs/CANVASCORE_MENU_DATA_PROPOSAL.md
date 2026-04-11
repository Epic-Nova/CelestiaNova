# CanvasCore Data-Based Menu System Proposal (Pre-Implementation)

This proposal defines a data-driven menu model for CanvasCore so extensions can describe menus in JSON with minimal C++ code.

The resolver model is decentralized.

- Any extension may define requirement keys.
- Any extension may resolve requirement keys.
- Any dynamic field may request values from a resolver extension.
- ContentForge and AstraScaleCore are examples of resolvers, not mandatory central components.

## Goals

- Define menus with structs, enums, and JSON instead of hardcoded per-page logic.
- Make dynamic values universal for all eligible fields.
- Keep requirement definitions and resolver behavior extension-owned.
- Keep UX aligned with existing FTXUI menu conventions, copy the style existent under "Menus"
- Keep parsing contracts in Core for reuse.

## Non-Negotiable Rules

- Dynamic option lists must come from requirementBinding and resolvers.
- Menu JSON files must not hardcode framework, environment, or similar dynamic provider lists.
- serviceCapabilities are discovery metadata and not equivalent to menu input contracts.
- Use only requirementBinding in menu JSON.

## Universal Dynamic Input Pattern

Any field that supports dynamic values can bind to a requirement key and ask a resolver extension for options.

Examples:

- framework.target
- environment.target
- database.target
- auth.provider
- gateway.policy

A field does not care which extension resolves the key. It only declares requirementBinding.

## High-Level Flow

1. An extension ships PLUGINNAME_MenuDefinitions.json.
2. CanvasCore loads menu definitions.
3. For each field with requirementBinding, CanvasCore builds a normalized resolve request.
4. Resolver registry selects resolver extension(s) for the key.
5. Resolver executes its strategy (API request, lookup, content query, or custom logic).
6. Resolver returns normalized options.
7. CanvasCore renders options in Dropdown or Radiobox or Menu and stores the selected value.
8. Submit payload and typed output structs are produced from selected values.

## Resolver Execution and Return Contract

CanvasCore should treat resolver calls as a universal protocol.

### Normalized resolve request shape

```json
{
  "correlationId": "resolve-8f2b",
  "consumerExtensionId": "canvascore",
  "menuId": "novaapi_content_setup",
  "fieldId": "targetEnvironment",
  "binding": {
    "key": "environment.target",
    "source": "AnyExtension",
    "resolveMode": "ValueList",
    "strategy": "ApiRequest",
    "requestAction": "astrascale.environments.list",
    "responseCollectionPath": "environments",
    "responseLabelPath": "displayName",
    "responseValuePath": "name"
  },
  "contextValues": {
    "frameworkProvider": "laravelorchestrator",
    "fallbackEnvironment": "production"
  }
}
```

### Normalized resolve response shape

```json
{
  "success": true,
  "resolverExtensionId": "astrascalecore",
  "options": [
    {
      "label": "Production",
      "value": "production",
      "description": "Live workload environment",
      "metadata": {
        "region": "eu-central-1"
      }
    },
    {
      "label": "Staging",
      "value": "staging",
      "description": "Pre-release validation",
      "metadata": {
        "region": "eu-central-1"
      }
    }
  ]
}
```

Failure response:

```json
{
  "success": false,
  "resolverExtensionId": "astrascalecore",
  "errorCode": "ResolverUnavailable",
  "errorMessage": "Environment catalog API is not reachable",
  "options": []
}
```

## AstraScaleCore Environment Example (Universal Pattern)

AstraScaleCore can implement environment.target resolution by querying existing environments and returning normalized options.

Universal behavior:

- Menu surface binds to environment.target.
- Resolver registry may prefer AstraScaleCore for that key.
- Any other extension can later implement the same key contract without changing menu JSON.

## Service Capabilities vs Dynamic Input Definitions

These are separate concerns.

- serviceCapabilities:
  - Runtime callable capability metadata.
  - Useful for discovery, monitoring, and integration maps.
- requirementDefinitions:
  - UI-facing dynamic input contracts.
  - Defines expected value mode, strategy hints, and request or output contracts.

Rule:

- Do not build dynamic menu field schemas directly from serviceCapabilities.
- Build dynamic fields from requirementDefinitions plus requirementBinding.

## Extension Descriptor Additions (Proposed)

Extensions may expose optional requirement metadata in their descriptors.

```json
{
  "id": "astrascalecore",
  "requirementDefinitions": [
    {
      "key": "environment.target",
      "title": "Deployment Environment",
      "inputType": "ProviderSelect",
      "resolveMode": "ValueList",
      "strategy": "ApiRequest",
      "requestAction": "astrascale.environments.list",
      "responseCollectionPath": "environments",
      "responseLabelPath": "displayName",
      "responseValuePath": "name",
      "requestStruct": {
        "name": "FEnvironmentLookupRequest"
      },
      "responseStruct": {
        "name": "FEnvironmentSelection"
      }
    }
  ],
  "requirementResolver": {
    "supportedKeys": [
      "environment.target",
      "scaling.policy"
    ]
  }
}
```

## Requirement Keys (Current Candidate Set)

- content.injection
- framework.target
- environment.target
- orchestrator.profile.depth
- orchestrator.setup.surface
- orchestrator.lifecycle.action
- database.target
- database.credentials.injection
- environment.policy
- scaling.policy
- auth.provider
- gateway.policy
- loadbalancer.policy
- firewall.policy
- cdn.policy
- telemetry.stream
- health.channel
- content.endpoint
- service.target
- extension.target

## FTXUI Coverage Strategy

The schema should cover interactive components and common visual/statistical DOM elements.

Interactive mapping:

- Bool -> Checkbox
- String -> Input
- PasswordString -> Input with password mode
- Integer and Float -> Input with numeric validation
- ProviderSelect -> Dropdown or Radiobox
- ProviderMultiSelect -> Menu multi-select model
- ToggleGroup -> Toggle
- SliderInt and SliderFloat -> Slider
- ActionButton -> Button
- CollapsibleSection -> Collapsible
- WindowPane -> Window
- ResizableSplit -> ResizableSplit
- StructObject -> grouped typed fields mapped into output structs

Visual mapping:

- TextLabel -> text or vtext or paragraph
- Separator -> separator variants
- ProgressGauge -> gauge
- DirectionalGauge -> gaugeLeft or gaugeRight or gaugeUp or gaugeDown
- SparklineGraph -> graph
- Spinner -> spinner
- Table -> ftxui::Table
- CanvasChart -> canvas

## Proposed Core Enums

```cpp
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
```

## Proposed Core Structs

```cpp
struct FCanvasResolveContextBinding {
    std::string requestKey;
    std::string fromFieldId;
    std::string fallbackValue;
};

struct FCanvasRequirementBinding {
    std::string key;
    ECanvasProviderSource source = ECanvasProviderSource::AnyExtension;
    ECanvasRequirementResolveMode resolveMode = ECanvasRequirementResolveMode::ProviderList;
    ECanvasResolverExecutionStrategy strategy = ECanvasResolverExecutionStrategy::RegistryLookup;

    bool allowMultiple = false;
    std::vector<std::string> preferredResolverExtensionIds;

    std::string requestAction;
    std::string responseCollectionPath;
    std::string responseLabelPath = "label";
    std::string responseValuePath = "value";
    std::vector<FCanvasResolveContextBinding> contextBindings;

    std::string requestStructName;
    std::string outputStructName;
};

struct FCanvasResolvedOption {
    std::string label;
    std::string value;
    std::string description;
    std::vector<std::pair<std::string, std::string>> metadata;
};

struct FCanvasRequirementResolveResult {
    bool success = false;
    std::string resolverExtensionId;
    std::string errorCode;
    std::string errorMessage;
    std::vector<FCanvasResolvedOption> options;
};
```

## JSON Schema and Autocomplete

Schema file:

- Extensions/Core/CanvasCore/MenuDefinitions/CanvasMenuDefinitions.schema.json

VS Code mapping:

- Match **/*_MenuDefinitions.json in workspace settings.

Result:

- Validation and autocomplete for requirementBinding, strategy, contextBindings, and field types.

## JSON File Contract Proposal

```json
{
  "$schema": "./CanvasMenuDefinitions.schema.json",
  "schema": "canvas.menu.definitions.v1",
  "ownerExtensionId": "canvascore",
  "menus": []
}
```

## Example: Universal Dynamic Resolution in Menu JSON

```json
{
  "id": "novaapi_content_setup",
  "title": "NovaAPI Content Setup",
  "pageRequirements": [
    "content.injection",
    "framework.target",
    "environment.target"
  ],
  "sections": [
    {
      "id": "dynamic_inputs",
      "title": "Dynamic Inputs",
      "fields": [
        {
          "id": "targetFrameworkProvider",
          "label": "Target Framework Provider",
          "type": "ProviderSelect",
          "requirementBinding": {
            "key": "framework.target",
            "source": "Orchestrator",
            "resolveMode": "ProviderList",
            "strategy": "RegistryLookup",
            "preferredResolverExtensionIds": ["contentforge"]
          },
          "outputKey": "novaApi.content.frameworkProvider"
        },
        {
          "id": "targetEnvironment",
          "label": "Target Environment",
          "type": "ProviderSelect",
          "requirementBinding": {
            "key": "environment.target",
            "source": "AnyExtension",
            "resolveMode": "ValueList",
            "strategy": "ApiRequest",
            "requestAction": "astrascale.environments.list",
            "responseCollectionPath": "environments",
            "responseLabelPath": "displayName",
            "responseValuePath": "name",
            "preferredResolverExtensionIds": ["astrascalecore"],
            "contextBindings": [
              { "requestKey": "frameworkProvider", "fromFieldId": "targetFrameworkProvider" },
              { "requestKey": "fallbackEnvironment", "fallbackValue": "production" }
            ]
          },
          "outputKey": "novaApi.content.environment"
        }
      ]
    }
  ]
}
```

## Resolution Rules for requirementBinding

Given requirementBinding.key = environment.target:

- CanvasCore creates a normalized resolve request.
- Resolver registry routes to compatible resolvers.
- Preferred resolver extension IDs are hints, not hard locks.
- Resolver returns normalized options and optional metadata.
- CanvasCore renders options and applies Required validation if no value is selected.

This flow is universal and applies to any dynamic field.

## Descriptor Resolution Strategy (Self and Others)

Resolvers often need data from their own descriptor and from other extension descriptors.

Recommended strategy:

1. Resolve descriptor path from registry using extension ID.
2. Parse descriptor JSON once and cache per extension ID.
3. Read self descriptor first for defaultResponses and local resolver config.
4. Read other descriptors only for allowlisted IDs from descriptorResolutionStrategy.others.
5. Fallback to registry list only when direct ID lookup fails.

Core APIs to use:

- PluginRegistry::GetDescriptorPath(extensionId)
- PluginRegistry::ListDescriptors()
- NovaFileOperations::ReadTextFile(path)

This avoids filesystem guesswork and keeps descriptor access centralized.

## Resolver ABI Strategy (Typed First, Optional Pointer Bridge)

Preferred approach:

- Use typed C++ request and response structs inside the module boundary.
- Return normalized options via typed result object.

Optional bridge:

- Export a C ABI helper that accepts void pointers only as transport wrappers.
- Immediately cast to typed request and response structs in implementation.

Guideline:

- Do not use raw void pointer payloads as the primary contract model.
- Keep canonical contract typed and versioned; treat pointer bridge as interop glue.

## Parser Placement (Core, Reusable)

JSON-to-struct parser contracts remain in Core.

- Source/Public/Core/IJsonStructParser.h

CanvasCore implements menu parser and schema validation using Core interfaces.

## Authoring Principle (Low-Code)

To add a setup page:

1. Add one menu JSON file.
2. Bind dynamic fields with requirementBinding.
3. Define resolver hints, strategy, and contextBindings only where needed.
4. Keep outputKey mapping stable for downstream services.

No page-specific C++ should be required for common dynamic forms.
