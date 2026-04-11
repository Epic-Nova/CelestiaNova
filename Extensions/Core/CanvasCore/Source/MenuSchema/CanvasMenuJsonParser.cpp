#include "MenuSchema/CanvasMenuJsonParser.h"

#include <unordered_map>

namespace CanvasCore::MenuSchema {
namespace {

using Core::EJsonParseSeverity;
using Core::FJsonParseIssue;
using nlohmann::json;

void AddIssue(std::vector<FJsonParseIssue>& issues,
              EJsonParseSeverity severity,
              const std::string& path,
              const std::string& message) {
    issues.push_back({severity, path, message});
}

bool HasErrors(const std::vector<FJsonParseIssue>& issues) {
    for (const FJsonParseIssue& issue : issues) {
        if (issue.Severity == EJsonParseSeverity::Error) {
            return true;
        }
    }
    return false;
}

std::string ChildPath(const std::string& base, const std::string& child) {
    if (base.empty()) {
        return child;
    }
    if (child.empty()) {
        return base;
    }
    return base + "." + child;
}

std::string IndexedPath(const std::string& base, const std::string& indexLabel, std::size_t index) {
    return base + "[" + std::to_string(index) + "]" + indexLabel;
}

bool ReadRequiredString(const json& object,
                        const std::string& key,
                        const std::string& objectPath,
                        std::string& outValue,
                        std::vector<FJsonParseIssue>& outIssues) {
    const auto it = object.find(key);
    const std::string fieldPath = ChildPath(objectPath, key);
    if (it == object.end()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, fieldPath, "Missing required string field.");
        return false;
    }
    if (!it->is_string()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, fieldPath, "Expected string value.");
        return false;
    }

    outValue = it->get<std::string>();
    if (outValue.empty()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, fieldPath, "Value cannot be empty.");
        return false;
    }

    return true;
}

bool ReadOptionalString(const json& object,
                        const std::string& key,
                        const std::string& objectPath,
                        std::string& outValue,
                        std::vector<FJsonParseIssue>& outIssues) {
    const auto it = object.find(key);
    if (it == object.end() || it->is_null()) {
        return true;
    }

    if (!it->is_string()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, ChildPath(objectPath, key), "Expected string value.");
        return false;
    }

    outValue = it->get<std::string>();
    return true;
}

bool ReadOptionalBool(const json& object,
                      const std::string& key,
                      const std::string& objectPath,
                      bool& outValue,
                      std::vector<FJsonParseIssue>& outIssues) {
    const auto it = object.find(key);
    if (it == object.end() || it->is_null()) {
        return true;
    }

    if (!it->is_boolean()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, ChildPath(objectPath, key), "Expected boolean value.");
        return false;
    }

    outValue = it->get<bool>();
    return true;
}

template <typename TEnum>
bool ReadOptionalEnum(const json& object,
                      const std::string& key,
                      const std::string& objectPath,
                      const std::unordered_map<std::string, TEnum>& lookup,
                      TEnum& outValue,
                      std::vector<FJsonParseIssue>& outIssues) {
    std::string raw;
    if (!ReadOptionalString(object, key, objectPath, raw, outIssues)) {
        return false;
    }

    if (raw.empty()) {
        return true;
    }

    const auto match = lookup.find(raw);
    if (match == lookup.end()) {
        AddIssue(outIssues,
                 EJsonParseSeverity::Error,
                 ChildPath(objectPath, key),
                 "Unknown enum value '" + raw + "'.");
        return false;
    }

    outValue = match->second;
    return true;
}

const std::unordered_map<std::string, ECanvasFieldType>& FieldTypeLookup() {
    static const std::unordered_map<std::string, ECanvasFieldType> kLookup = {
        {"Bool", ECanvasFieldType::Bool},
        {"String", ECanvasFieldType::String},
        {"PasswordString", ECanvasFieldType::PasswordString},
        {"Integer", ECanvasFieldType::Integer},
        {"Float", ECanvasFieldType::Float},
        {"ProviderSelect", ECanvasFieldType::ProviderSelect},
        {"ProviderMultiSelect", ECanvasFieldType::ProviderMultiSelect},
        {"ToggleGroup", ECanvasFieldType::ToggleGroup},
        {"RadioGroup", ECanvasFieldType::RadioGroup},
        {"Dropdown", ECanvasFieldType::Dropdown},
        {"SliderInt", ECanvasFieldType::SliderInt},
        {"SliderFloat", ECanvasFieldType::SliderFloat},
        {"ActionButton", ECanvasFieldType::ActionButton},
        {"TextLabel", ECanvasFieldType::TextLabel},
        {"Paragraph", ECanvasFieldType::Paragraph},
        {"Separator", ECanvasFieldType::Separator},
        {"Spacer", ECanvasFieldType::Spacer},
        {"Table", ECanvasFieldType::Table},
        {"ProgressGauge", ECanvasFieldType::ProgressGauge},
        {"DirectionalGauge", ECanvasFieldType::DirectionalGauge},
        {"SparklineGraph", ECanvasFieldType::SparklineGraph},
        {"Spinner", ECanvasFieldType::Spinner},
        {"CanvasChart", ECanvasFieldType::CanvasChart},
        {"StructObject", ECanvasFieldType::StructObject},
        {"WindowPane", ECanvasFieldType::WindowPane},
        {"CollapsibleSection", ECanvasFieldType::CollapsibleSection},
        {"ResizableSplit", ECanvasFieldType::ResizableSplit},
        {"TabContainer", ECanvasFieldType::TabContainer},
    };
    return kLookup;
}

const std::unordered_map<std::string, ECanvasProviderSource>& ProviderSourceLookup() {
    static const std::unordered_map<std::string, ECanvasProviderSource> kLookup = {
        {"AnyExtension", ECanvasProviderSource::AnyExtension},
        {"Core", ECanvasProviderSource::Core},
        {"Agent", ECanvasProviderSource::Agent},
        {"Service", ECanvasProviderSource::Service},
        {"Orchestrator", ECanvasProviderSource::Orchestrator},
    };
    return kLookup;
}

const std::unordered_map<std::string, ECanvasRequirementResolveMode>& ResolveModeLookup() {
    static const std::unordered_map<std::string, ECanvasRequirementResolveMode> kLookup = {
        {"ProviderList", ECanvasRequirementResolveMode::ProviderList},
        {"ValueList", ECanvasRequirementResolveMode::ValueList},
        {"StructuredObject", ECanvasRequirementResolveMode::StructuredObject},
    };
    return kLookup;
}

const std::unordered_map<std::string, ECanvasResolverExecutionStrategy>& ExecutionStrategyLookup() {
    static const std::unordered_map<std::string, ECanvasResolverExecutionStrategy> kLookup = {
        {"RegistryLookup", ECanvasResolverExecutionStrategy::RegistryLookup},
        {"ApiRequest", ECanvasResolverExecutionStrategy::ApiRequest},
        {"ContentQuery", ECanvasResolverExecutionStrategy::ContentQuery},
        {"Custom", ECanvasResolverExecutionStrategy::Custom},
    };
    return kLookup;
}

const std::unordered_map<std::string, ECanvasValueKind>& ValueKindLookup() {
    static const std::unordered_map<std::string, ECanvasValueKind> kLookup = {
        {"Bool", ECanvasValueKind::Bool},
        {"String", ECanvasValueKind::String},
        {"Integer", ECanvasValueKind::Integer},
        {"Float", ECanvasValueKind::Float},
        {"Object", ECanvasValueKind::Object},
        {"Array", ECanvasValueKind::Array},
    };
    return kLookup;
}

const std::unordered_map<std::string, ECanvasValueStorage>& ValueStorageLookup() {
    static const std::unordered_map<std::string, ECanvasValueStorage> kLookup = {
        {"RuntimeOnly", ECanvasValueStorage::RuntimeOnly},
        {"ConfigJson", ECanvasValueStorage::ConfigJson},
        {"Session", ECanvasValueStorage::Session},
        {"SecretStore", ECanvasValueStorage::SecretStore},
    };
    return kLookup;
}

const std::unordered_map<std::string, ECanvasValidationKind>& ValidationKindLookup() {
    static const std::unordered_map<std::string, ECanvasValidationKind> kLookup = {
        {"None", ECanvasValidationKind::None},
        {"Required", ECanvasValidationKind::Required},
        {"Min", ECanvasValidationKind::Min},
        {"Max", ECanvasValidationKind::Max},
        {"Regex", ECanvasValidationKind::Regex},
        {"ExistingPath", ECanvasValidationKind::ExistingPath},
        {"ExistingDirectory", ECanvasValidationKind::ExistingDirectory},
    };
    return kLookup;
}

bool ParseResolveContextBinding(const json& node,
                                const std::string& path,
                                FCanvasResolveContextBinding& outBinding,
                                std::vector<FJsonParseIssue>& outIssues) {
    if (!node.is_object()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, path, "Expected context binding object.");
        return false;
    }

    bool ok = true;
    ok &= ReadRequiredString(node, "requestKey", path, outBinding.RequestKey, outIssues);
    ok &= ReadOptionalString(node, "fromFieldId", path, outBinding.FromFieldId, outIssues);
    ok &= ReadOptionalString(node, "fallbackValue", path, outBinding.FallbackValue, outIssues);
    return ok;
}

bool ParseRequirementBinding(const json& node,
                             const std::string& path,
                             FCanvasRequirementBinding& outBinding,
                             std::vector<FJsonParseIssue>& outIssues) {
    if (!node.is_object()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, path, "Expected requirement binding object.");
        return false;
    }

    bool ok = true;
    ok &= ReadRequiredString(node, "key", path, outBinding.RequirementKey, outIssues);
    ok &= ReadOptionalEnum(node, "source", path, ProviderSourceLookup(), outBinding.Source, outIssues);
    ok &= ReadOptionalEnum(node, "resolveMode", path, ResolveModeLookup(), outBinding.ResolveMode, outIssues);
    ok &= ReadOptionalEnum(node, "strategy", path, ExecutionStrategyLookup(), outBinding.Strategy, outIssues);
    ok &= ReadOptionalBool(node, "allowMultiple", path, outBinding.AllowMultiple, outIssues);

    ReadOptionalString(node, "requestAction", path, outBinding.RequestAction, outIssues);
    ReadOptionalString(node, "responseCollectionPath", path, outBinding.ResponseCollectionPath, outIssues);
    ReadOptionalString(node, "responseLabelPath", path, outBinding.ResponseLabelPath, outIssues);
    ReadOptionalString(node, "responseValuePath", path, outBinding.ResponseValuePath, outIssues);
    ReadOptionalString(node, "requestStructName", path, outBinding.RequestStructName, outIssues);
    ReadOptionalString(node, "outputStructName", path, outBinding.OutputStructName, outIssues);

    const auto preferredIt = node.find("preferredResolverExtensionIds");
    if (preferredIt != node.end() && !preferredIt->is_null()) {
        if (!preferredIt->is_array()) {
            AddIssue(outIssues,
                     EJsonParseSeverity::Error,
                     ChildPath(path, "preferredResolverExtensionIds"),
                     "Expected string array.");
            ok = false;
        } else {
            for (std::size_t idx = 0; idx < preferredIt->size(); ++idx) {
                const json& value = (*preferredIt)[idx];
                if (!value.is_string()) {
                    AddIssue(outIssues,
                             EJsonParseSeverity::Error,
                             IndexedPath(ChildPath(path, "preferredResolverExtensionIds"), "", idx),
                             "Expected string value.");
                    ok = false;
                    continue;
                }
                const std::string extensionId = value.get<std::string>();
                if (!extensionId.empty()) {
                    outBinding.PreferredResolverExtensionIds.push_back(extensionId);
                }
            }
        }
    }

    const auto contextBindingsIt = node.find("contextBindings");
    if (contextBindingsIt != node.end() && !contextBindingsIt->is_null()) {
        if (!contextBindingsIt->is_array()) {
            AddIssue(outIssues,
                     EJsonParseSeverity::Error,
                     ChildPath(path, "contextBindings"),
                     "Expected object array.");
            ok = false;
        } else {
            for (std::size_t idx = 0; idx < contextBindingsIt->size(); ++idx) {
                FCanvasResolveContextBinding binding;
                const std::string bindingPath = IndexedPath(ChildPath(path, "contextBindings"), "", idx);
                if (!ParseResolveContextBinding((*contextBindingsIt)[idx], bindingPath, binding, outIssues)) {
                    ok = false;
                    continue;
                }
                outBinding.ContextBindings.push_back(std::move(binding));
            }
        }
    }

    return ok;
}

bool ParseValidationDefinition(const json& node,
                               const std::string& path,
                               FCanvasFieldValidation& outValidation,
                               std::vector<FJsonParseIssue>& outIssues) {
    if (!node.is_object()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, path, "Expected validation object.");
        return false;
    }

    bool ok = true;
    ok &= ReadOptionalEnum(node, "kind", path, ValidationKindLookup(), outValidation.Kind, outIssues);

    if (node.find("kind") == node.end()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, ChildPath(path, "kind"), "Missing required validation kind.");
        ok = false;
    }

    ReadOptionalString(node, "argument", path, outValidation.Argument, outIssues);
    ReadOptionalString(node, "message", path, outValidation.Message, outIssues);
    return ok;
}

bool ParseStructFieldDefinition(const json& node,
                                const std::string& path,
                                FCanvasStructFieldDefinition& outField,
                                std::vector<FJsonParseIssue>& outIssues) {
    if (!node.is_object()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, path, "Expected struct field object.");
        return false;
    }

    bool ok = true;
    ok &= ReadRequiredString(node, "id", path, outField.Id, outIssues);
    ok &= ReadRequiredString(node, "label", path, outField.Label, outIssues);
    ReadOptionalString(node, "description", path, outField.Description, outIssues);
    ok &= ReadOptionalEnum(node, "valueKind", path, ValueKindLookup(), outField.ValueKind, outIssues);
    if (node.find("valueKind") == node.end()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, ChildPath(path, "valueKind"), "Missing required valueKind.");
        ok = false;
    }

    ReadOptionalBool(node, "required", path, outField.Required, outIssues);
    ReadOptionalString(node, "defaultValue", path, outField.DefaultValue, outIssues);

    const auto childrenIt = node.find("children");
    if (childrenIt != node.end() && !childrenIt->is_null()) {
        if (!childrenIt->is_array()) {
            AddIssue(outIssues,
                     EJsonParseSeverity::Error,
                     ChildPath(path, "children"),
                     "Expected struct field array.");
            ok = false;
        } else {
            for (std::size_t idx = 0; idx < childrenIt->size(); ++idx) {
                FCanvasStructFieldDefinition child;
                const std::string childPath = IndexedPath(ChildPath(path, "children"), "", idx);
                if (!ParseStructFieldDefinition((*childrenIt)[idx], childPath, child, outIssues)) {
                    ok = false;
                    continue;
                }
                outField.Children.push_back(std::move(child));
            }
        }
    }

    return ok;
}

bool ParseFieldDefinition(const json& node,
                          const std::string& path,
                          FCanvasFieldDefinition& outField,
                          std::vector<FJsonParseIssue>& outIssues) {
    if (!node.is_object()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, path, "Expected field object.");
        return false;
    }

    bool ok = true;
    ok &= ReadRequiredString(node, "id", path, outField.Id, outIssues);
    ok &= ReadRequiredString(node, "label", path, outField.Label, outIssues);
    ReadOptionalString(node, "description", path, outField.Description, outIssues);
    ok &= ReadOptionalEnum(node, "type", path, FieldTypeLookup(), outField.Type, outIssues);

    if (node.find("type") == node.end()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, ChildPath(path, "type"), "Missing required field type.");
        ok = false;
    }

    ReadOptionalString(node, "defaultValue", path, outField.DefaultValue, outIssues);
    ReadOptionalString(node, "dataRequirementKey", path, outField.DataRequirementKey, outIssues);
    ReadOptionalString(node, "renderTemplate", path, outField.RenderTemplate, outIssues);
    ReadOptionalString(node, "visibleIfField", path, outField.VisibleIfField, outIssues);
    ReadOptionalString(node, "visibleIfEquals", path, outField.VisibleIfEquals, outIssues);
    ReadOptionalString(node, "outputKey", path, outField.OutputKey, outIssues);
    ReadOptionalEnum(node, "valueStorage", path, ValueStorageLookup(), outField.ValueStorage, outIssues);

    const auto requirementIt = node.find("requirementBinding");
    if (requirementIt != node.end() && !requirementIt->is_null()) {
        FCanvasRequirementBinding binding;
        if (ParseRequirementBinding(*requirementIt, ChildPath(path, "requirementBinding"), binding, outIssues)) {
            outField.RequirementBinding = std::move(binding);
        } else {
            ok = false;
        }
    }

    const auto structFieldsIt = node.find("structFields");
    if (structFieldsIt != node.end() && !structFieldsIt->is_null()) {
        if (!structFieldsIt->is_array()) {
            AddIssue(outIssues,
                     EJsonParseSeverity::Error,
                     ChildPath(path, "structFields"),
                     "Expected struct field array.");
            ok = false;
        } else {
            for (std::size_t idx = 0; idx < structFieldsIt->size(); ++idx) {
                FCanvasStructFieldDefinition structField;
                const std::string structPath = IndexedPath(ChildPath(path, "structFields"), "", idx);
                if (!ParseStructFieldDefinition((*structFieldsIt)[idx], structPath, structField, outIssues)) {
                    ok = false;
                    continue;
                }
                outField.StructFields.push_back(std::move(structField));
            }
        }
    }

    const auto validationsIt = node.find("validations");
    if (validationsIt != node.end() && !validationsIt->is_null()) {
        if (!validationsIt->is_array()) {
            AddIssue(outIssues,
                     EJsonParseSeverity::Error,
                     ChildPath(path, "validations"),
                     "Expected validation array.");
            ok = false;
        } else {
            for (std::size_t idx = 0; idx < validationsIt->size(); ++idx) {
                FCanvasFieldValidation validation;
                const std::string validationPath = IndexedPath(ChildPath(path, "validations"), "", idx);
                if (!ParseValidationDefinition((*validationsIt)[idx], validationPath, validation, outIssues)) {
                    ok = false;
                    continue;
                }
                outField.Validations.push_back(std::move(validation));
            }
        }
    }

    return ok;
}

bool ParseSectionDefinition(const json& node,
                            const std::string& path,
                            FCanvasSectionDefinition& outSection,
                            std::vector<FJsonParseIssue>& outIssues) {
    if (!node.is_object()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, path, "Expected section object.");
        return false;
    }

    bool ok = true;
    ok &= ReadRequiredString(node, "id", path, outSection.Id, outIssues);
    ok &= ReadRequiredString(node, "title", path, outSection.Title, outIssues);
    ReadOptionalString(node, "description", path, outSection.Description, outIssues);

    const auto fieldsIt = node.find("fields");
    if (fieldsIt == node.end() || !fieldsIt->is_array()) {
        AddIssue(outIssues,
                 EJsonParseSeverity::Error,
                 ChildPath(path, "fields"),
                 "Missing required field array.");
        return false;
    }

    for (std::size_t idx = 0; idx < fieldsIt->size(); ++idx) {
        FCanvasFieldDefinition field;
        const std::string fieldPath = IndexedPath(ChildPath(path, "fields"), "", idx);
        if (!ParseFieldDefinition((*fieldsIt)[idx], fieldPath, field, outIssues)) {
            ok = false;
            continue;
        }
        outSection.Fields.push_back(std::move(field));
    }

    return ok;
}

bool ParseMenuDefinition(const json& node,
                         const std::string& path,
                         FCanvasMenuDefinition& outMenu,
                         std::vector<FJsonParseIssue>& outIssues) {
    if (!node.is_object()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, path, "Expected menu object.");
        return false;
    }

    bool ok = true;
    ok &= ReadRequiredString(node, "id", path, outMenu.Id, outIssues);
    ok &= ReadRequiredString(node, "title", path, outMenu.Title, outIssues);
    ReadOptionalString(node, "subtitle", path, outMenu.Subtitle, outIssues);
    ReadOptionalString(node, "icon", path, outMenu.Icon, outIssues);
    ReadOptionalString(node, "submitAction", path, outMenu.SubmitAction, outIssues);
    ReadOptionalString(node, "cancelAction", path, outMenu.CancelAction, outIssues);

    const auto requirementsIt = node.find("pageRequirements");
    if (requirementsIt != node.end() && !requirementsIt->is_null()) {
        if (!requirementsIt->is_array()) {
            AddIssue(outIssues,
                     EJsonParseSeverity::Error,
                     ChildPath(path, "pageRequirements"),
                     "Expected string array.");
            ok = false;
        } else {
            for (std::size_t idx = 0; idx < requirementsIt->size(); ++idx) {
                const json& value = (*requirementsIt)[idx];
                if (!value.is_string()) {
                    AddIssue(outIssues,
                             EJsonParseSeverity::Error,
                             IndexedPath(ChildPath(path, "pageRequirements"), "", idx),
                             "Expected string value.");
                    ok = false;
                    continue;
                }
                const std::string requirement = value.get<std::string>();
                if (!requirement.empty()) {
                    outMenu.PageRequirements.push_back(requirement);
                }
            }
        }
    }

    const auto sectionsIt = node.find("sections");
    if (sectionsIt == node.end() || !sectionsIt->is_array()) {
        AddIssue(outIssues,
                 EJsonParseSeverity::Error,
                 ChildPath(path, "sections"),
                 "Missing required section array.");
        return false;
    }

    for (std::size_t idx = 0; idx < sectionsIt->size(); ++idx) {
        FCanvasSectionDefinition section;
        const std::string sectionPath = IndexedPath(ChildPath(path, "sections"), "", idx);
        if (!ParseSectionDefinition((*sectionsIt)[idx], sectionPath, section, outIssues)) {
            ok = false;
            continue;
        }
        outMenu.Sections.push_back(std::move(section));
    }

    return ok;
}

} // namespace

std::string CanvasMenuJsonParser::GetSchemaId() const {
    return "canvas.menu.definitions.v1";
}

bool CanvasMenuJsonParser::Validate(const nlohmann::json& root,
                                    std::vector<Core::FJsonParseIssue>& outIssues) const {
    if (!root.is_object()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, "$", "Menu definition root must be a JSON object.");
        return false;
    }

    if (!root.contains("schema")) {
        AddIssue(outIssues, EJsonParseSeverity::Error, "$.schema", "Missing required schema field.");
    } else if (!root["schema"].is_string()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, "$.schema", "Schema field must be a string.");
    }

    if (!root.contains("ownerExtensionId")) {
        AddIssue(outIssues, EJsonParseSeverity::Error, "$.ownerExtensionId", "Missing required ownerExtensionId field.");
    } else if (!root["ownerExtensionId"].is_string()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, "$.ownerExtensionId", "ownerExtensionId must be a string.");
    }

    if (!root.contains("menus")) {
        AddIssue(outIssues, EJsonParseSeverity::Error, "$.menus", "Missing required menus field.");
    } else if (!root["menus"].is_array()) {
        AddIssue(outIssues, EJsonParseSeverity::Error, "$.menus", "menus must be an array.");
    }

    return !HasErrors(outIssues);
}

bool CanvasMenuJsonParser::Parse(const nlohmann::json& root,
                                 FCanvasMenuFile& outValue,
                                 std::vector<Core::FJsonParseIssue>& outIssues) const {
    outValue = {};

    if (!Validate(root, outIssues)) {
        return false;
    }

    ReadRequiredString(root, "schema", "$", outValue.Schema, outIssues);
    ReadRequiredString(root, "ownerExtensionId", "$", outValue.OwnerExtensionId, outIssues);

    if (outValue.Schema != GetSchemaId()) {
        AddIssue(outIssues,
                 EJsonParseSeverity::Error,
                 "$.schema",
                 "Unsupported menu schema '" + outValue.Schema + "'. Expected '" + GetSchemaId() + "'.");
    }

    const auto menusIt = root.find("menus");
    if (menusIt != root.end() && menusIt->is_array()) {
        for (std::size_t idx = 0; idx < menusIt->size(); ++idx) {
            FCanvasMenuDefinition menu;
            const std::string menuPath = IndexedPath("$.menus", "", idx);
            if (!ParseMenuDefinition((*menusIt)[idx], menuPath, menu, outIssues)) {
                continue;
            }
            outValue.Menus.push_back(std::move(menu));
        }
    }

    return !HasErrors(outIssues);
}

} // namespace CanvasCore::MenuSchema
