#include "MenuSchema/CanvasMenuRuntime.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <unordered_set>

#include "Core/ExtensionDescriptorJson.h"
#include "Core/NovaFileOperations.h"
#include "Core/ExtensionRegistry.h"
#include "Core/NovaLog.h"
#include "Core/RequirementResolver.h"
#include "MenuSchema/CanvasMenuJsonParser.h"

#include "json.hpp"

namespace CanvasCore {
namespace {

using Core::EJsonParseSeverity;
using Core::FJsonParseIssue;
using nlohmann::json;

struct FResolverCandidate {
    std::string ExtensionId;
    std::string SourceRequirementKey;
    std::string ExportSymbol;
    bool SupportsKey = false;
    bool Authoritative = false;
    int PreferenceRank = INT_MAX;
    std::vector<std::string> AllowedRequestors;
    nlohmann::json DescriptorJson;
};

bool HasErrorIssues(const std::vector<FJsonParseIssue>& issues) {
    for (const FJsonParseIssue& issue : issues) {
        if (issue.Severity == EJsonParseSeverity::Error) {
            return true;
        }
    }
    return false;
}

void AddIssue(std::vector<FJsonParseIssue>& outIssues,
              EJsonParseSeverity severity,
              const std::string& path,
              const std::string& message) {
    outIssues.push_back({severity, path, message});
}

std::string BuildQualifiedMenuId(const std::string& ownerExtensionId, const std::string& menuId) {
    if (ownerExtensionId.empty()) {
        return menuId;
    }
    return ownerExtensionId + "::" + menuId;
}

std::unordered_map<std::string, std::string> BuildFieldValueMap(
    const std::vector<FCanvasFieldValue>& values) {
    std::unordered_map<std::string, std::string> byFieldId;
    byFieldId.reserve(values.size());

    for (const FCanvasFieldValue& value : values) {
        if (value.FieldId.empty()) {
            continue;
        }
        byFieldId[value.FieldId] = value.Value;
    }

    return byFieldId;
}

std::string GetLastPathSegment(const std::string& path) {
    if (path.empty()) {
        return "";
    }

    std::size_t pos = path.find_last_of("./");
    if (pos == std::string::npos) {
        return path;
    }

    if (pos + 1 >= path.size()) {
        return "";
    }

    return path.substr(pos + 1);
}

bool IsRequestorAllowed(const std::vector<std::string>& allowedRequestors,
                        const std::string& consumerExtensionId) {
    if (allowedRequestors.empty()) {
        return true;
    }

    if (std::find(allowedRequestors.begin(), allowedRequestors.end(), "*") != allowedRequestors.end()) {
        return true;
    }

    if (consumerExtensionId.empty()) {
        return false;
    }

    return std::find(allowedRequestors.begin(), allowedRequestors.end(), consumerExtensionId) != allowedRequestors.end();
}

int GetPreferenceRank(const std::vector<std::string>& preferredResolvers, const std::string& extensionId) {
    for (std::size_t index = 0; index < preferredResolvers.size(); ++index) {
        if (preferredResolvers[index] == extensionId) {
            return static_cast<int>(index);
        }
    }

    return INT_MAX;
}

bool ParseAuthoritativeRule(const nlohmann::json& ruleValue,
                            const std::string& descriptorId,
                            const std::string& requestedRequirementKey,
                            std::string& outSourceRequirementKey,
                            std::vector<std::string>& outAllowedRequestors) {
    outSourceRequirementKey = requestedRequirementKey;
    outAllowedRequestors.clear();

    if (ruleValue.is_boolean()) {
        return ruleValue.get<bool>();
    }

    if (ruleValue.is_string()) {
        const std::string sourceKey = ruleValue.get<std::string>();
        if (sourceKey.empty()) {
            return false;
        }
        outSourceRequirementKey = sourceKey;
        return true;
    }

    if (!ruleValue.is_object()) {
        return false;
    }

    if (ruleValue.contains("extensionId") && ruleValue["extensionId"].is_string()) {
        const std::string explicitOwner = ruleValue["extensionId"].get<std::string>();
        if (!explicitOwner.empty() && explicitOwner != descriptorId) {
            return false;
        }
    }

    if (ruleValue.contains("sourceRequirementKey") && ruleValue["sourceRequirementKey"].is_string()) {
        const std::string sourceKey = ruleValue["sourceRequirementKey"].get<std::string>();
        if (sourceKey.empty()) {
            return false;
        }
        outSourceRequirementKey = sourceKey;
    }

    if (ruleValue.contains("allowedRequestors")) {
        if (!ruleValue["allowedRequestors"].is_array()) {
            return false;
        }

        for (const auto& entry : ruleValue["allowedRequestors"]) {
            if (!entry.is_string()) {
                continue;
            }

            const std::string requestor = entry.get<std::string>();
            if (!requestor.empty()) {
                outAllowedRequestors.push_back(requestor);
            }
        }
    }

    return true;
}

bool ContainsSupportedKey(const nlohmann::json& descriptorJson, const std::string& requirementKey) {
    const auto canvasIt = descriptorJson.find("canvas");
    if (canvasIt == descriptorJson.end() || !canvasIt->is_object()) {
        return false;
    }

    const auto requirementsIt = canvasIt->find("requirements");
    if (requirementsIt == canvasIt->end() || !requirementsIt->is_object()) {
        return false;
    }

    const auto resolverIt = requirementsIt->find("resolver");
    if (resolverIt == requirementsIt->end() || !resolverIt->is_object()) {
        return false;
    }

    const auto supportedIt = resolverIt->find("supportedKeys");
    if (supportedIt == resolverIt->end() || !supportedIt->is_array()) {
        return false;
    }

    for (const auto& key : *supportedIt) {
        if (!key.is_string()) {
            continue;
        }
        if (key.get<std::string>() == requirementKey) {
            return true;
        }
    }

    return false;
}

std::string ReadResolverExportSymbol(const nlohmann::json& descriptorJson) {
    const auto canvasIt = descriptorJson.find("canvas");
    if (canvasIt == descriptorJson.end() || !canvasIt->is_object()) {
        return "";
    }

    const auto requirementsIt = canvasIt->find("requirements");
    if (requirementsIt == canvasIt->end() || !requirementsIt->is_object()) {
        return "";
    }

    const auto resolverIt = requirementsIt->find("resolver");
    if (resolverIt == requirementsIt->end() || !resolverIt->is_object()) {
        return "";
    }

    const auto apiIt = resolverIt->find("resolverApi");
    if (apiIt == resolverIt->end() || !apiIt->is_object()) {
        return "";
    }

    const auto bridgeIt = apiIt->find("cAbiBridge");
    if (bridgeIt == apiIt->end() || !bridgeIt->is_object()) {
        return "";
    }

    const auto exportIt = bridgeIt->find("export");
    if (exportIt == bridgeIt->end() || !exportIt->is_string()) {
        return "";
    }

    return exportIt->get<std::string>();
}

bool HasDefaultResponsesForKey(const nlohmann::json& descriptorJson,
                               const std::string& requirementKey,
                               const std::string& collectionKey) {
    if (requirementKey.empty() || collectionKey.empty()) {
        return false;
    }

    const auto options = Core::ExtensionDescriptorJson::ReadDefaultResolverOptions(
        descriptorJson,
        requirementKey,
        collectionKey);

    return !options.empty();
}

bool TryResolveAsBool(const std::string& input, bool& outValue) {
    if (input == "1" || input == "true" || input == "TRUE" || input == "True") {
        outValue = true;
        return true;
    }

    if (input == "0" || input == "false" || input == "FALSE" || input == "False") {
        outValue = false;
        return true;
    }

    return false;
}

bool TryResolveAsInteger(const std::string& input, std::int64_t& outValue) {
    if (input.empty()) {
        return false;
    }

    std::size_t index = 0;
    try {
        outValue = std::stoll(input, &index, 10);
    } catch (...) {
        return false;
    }

    return index == input.size();
}

bool TryResolveAsFloat(const std::string& input, double& outValue) {
    if (input.empty()) {
        return false;
    }

    std::size_t index = 0;
    try {
        outValue = std::stod(input, &index);
    } catch (...) {
        return false;
    }

    return index == input.size();
}

nlohmann::json ConvertValueKind(const MenuSchema::ECanvasValueKind valueKind,
                                const std::string& value,
                                bool& outSuccess) {
    outSuccess = true;

    switch (valueKind) {
        case MenuSchema::ECanvasValueKind::Bool: {
            bool resolved = false;
            if (!TryResolveAsBool(value, resolved)) {
                outSuccess = false;
                return value;
            }
            return resolved;
        }
        case MenuSchema::ECanvasValueKind::Integer: {
            std::int64_t resolved = 0;
            if (!TryResolveAsInteger(value, resolved)) {
                outSuccess = false;
                return value;
            }
            return resolved;
        }
        case MenuSchema::ECanvasValueKind::Float: {
            double resolved = 0.0;
            if (!TryResolveAsFloat(value, resolved)) {
                outSuccess = false;
                return value;
            }
            return resolved;
        }
        case MenuSchema::ECanvasValueKind::Object:
        case MenuSchema::ECanvasValueKind::Array: {
            if (value.empty()) {
                return valueKind == MenuSchema::ECanvasValueKind::Object ? nlohmann::json::object()
                                                                          : nlohmann::json::array();
            }

            try {
                nlohmann::json parsed = nlohmann::json::parse(value);
                if (valueKind == MenuSchema::ECanvasValueKind::Object && parsed.is_object()) {
                    return parsed;
                }
                if (valueKind == MenuSchema::ECanvasValueKind::Array && parsed.is_array()) {
                    return parsed;
                }
            } catch (...) {
                outSuccess = false;
                return value;
            }

            outSuccess = false;
            return value;
        }
        case MenuSchema::ECanvasValueKind::String:
        default:
            return value;
    }
}

bool SetJsonPathValue(nlohmann::json& root, const std::string& path, const nlohmann::json& value) {
    if (path.empty()) {
        return false;
    }

    std::vector<std::string> segments;
    std::string current;
    for (char ch : path) {
        if (ch == '.') {
            if (!current.empty()) {
                segments.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        segments.push_back(current);
    }

    if (segments.empty()) {
        return false;
    }

    nlohmann::json* node = &root;
    for (std::size_t idx = 0; idx < segments.size(); ++idx) {
        const std::string& segment = segments[idx];
        if (segment.empty()) {
            return false;
        }

        const bool isLeaf = idx + 1 == segments.size();
        if (isLeaf) {
            (*node)[segment] = value;
            return true;
        }

        nlohmann::json& next = (*node)[segment];
        if (!next.is_object()) {
            next = nlohmann::json::object();
        }
        node = &next;
    }

    return false;
}

nlohmann::json BuildStructObjectValue(const MenuSchema::FCanvasFieldDefinition& field,
                                      const std::unordered_map<std::string, std::string>& valuesByField,
                                      std::string& outError) {
    nlohmann::json objectValue = nlohmann::json::object();

    for (const auto& structField : field.StructFields) {
        const std::string compoundFieldId = field.Id + "." + structField.Id;

        std::string rawValue;
        auto provided = valuesByField.find(compoundFieldId);
        if (provided != valuesByField.end()) {
            rawValue = provided->second;
        } else {
            rawValue = structField.DefaultValue;
        }

        if (structField.Required && rawValue.empty()) {
            outError = "Missing required struct field value for '" + compoundFieldId + "'.";
            return nlohmann::json();
        }

        bool converted = true;
        nlohmann::json convertedValue = ConvertValueKind(structField.ValueKind, rawValue, converted);
        if (!converted) {
            outError = "Invalid value for struct field '" + compoundFieldId + "'.";
            return nlohmann::json();
        }

        if (!structField.Children.empty()) {
            if (!convertedValue.is_object()) {
                convertedValue = nlohmann::json::object();
            }

            for (const auto& child : structField.Children) {
                const std::string childFieldId = compoundFieldId + "." + child.Id;

                std::string childRawValue;
                auto childValue = valuesByField.find(childFieldId);
                if (childValue != valuesByField.end()) {
                    childRawValue = childValue->second;
                } else {
                    childRawValue = child.DefaultValue;
                }

                if (child.Required && childRawValue.empty()) {
                    outError = "Missing required struct field value for '" + childFieldId + "'.";
                    return nlohmann::json();
                }

                bool childConverted = true;
                nlohmann::json childJson = ConvertValueKind(child.ValueKind, childRawValue, childConverted);
                if (!childConverted) {
                    outError = "Invalid value for struct field '" + childFieldId + "'.";
                    return nlohmann::json();
                }

                convertedValue[child.Id] = childJson;
            }
        }

        objectValue[structField.Id] = convertedValue;
    }

    return objectValue;
}

bool FieldRequiresValue(const MenuSchema::FCanvasFieldDefinition& field) {
    for (const auto& validation : field.Validations) {
        if (validation.Kind == MenuSchema::ECanvasValidationKind::Required) {
            return true;
        }
    }

    return false;
}

} // namespace

bool CanvasMenuRuntime::ReloadMenuDefinitions(std::vector<Core::FJsonParseIssue>& outIssues) {
    Documents_.clear();
    QualifiedMenuRefs_.clear();
    MenuRefsById_.clear();

    auto& registry = Core::ExtensionRegistry::Instance();
    const auto descriptors = registry.ListExtensionDescriptors();

    for (const auto& descriptor : descriptors) {
        NOVA_LOG(("[CanvasMenuRuntime] Processing extension for menus: " + descriptor.id).c_str(), LogType::Log);
        const nlohmann::json descriptorJson = Core::ExtensionDescriptorJson::LoadDescriptorJsonById(descriptor.id);
        if (!descriptorJson.is_object()) {
            NOVA_LOG(("[CanvasMenuRuntime] No descriptor JSON found for: " + descriptor.id).c_str(), LogType::Warning);
            continue;
        }

        const auto canvasIt = descriptorJson.find("canvas");
        if (canvasIt == descriptorJson.end() || !canvasIt->is_object()) {
            continue;
        }

        const auto menuDefinitionsIt = canvasIt->find("menuDefinitions");
        if (menuDefinitionsIt == canvasIt->end() || !menuDefinitionsIt->is_object()) {
            NOVA_LOG(("[CanvasMenuRuntime] Extension " + descriptor.id + " has canvas block but no menuDefinitions").c_str(), LogType::Log);
            continue;
        }

        const std::string schema = menuDefinitionsIt->value("schema", "");
        if (schema != Parser_.GetSchemaId()) {
            AddIssue(outIssues,
                     EJsonParseSeverity::Warning,
                     descriptor.id + ".canvas.menuDefinitions.schema",
                     "Unsupported menu definitions schema '" + schema + "'.");
            continue;
        }

        const std::string menuDefinitionFile = menuDefinitionsIt->value("file", "");
        if (menuDefinitionFile.empty()) {
            AddIssue(outIssues,
                     EJsonParseSeverity::Error,
                     descriptor.id + ".canvas.menuDefinitions.file",
                     "Menu definition file path is missing.");
            continue;
        }

        const std::string descriptorPath = registry.GetExtensionDescriptorPath(descriptor.id);
        if (descriptorPath.empty()) {
            AddIssue(outIssues,
                     EJsonParseSeverity::Error,
                     descriptor.id,
                     "Descriptor path was not available in PluginRegistry.");
            continue;
        }

        const std::string descriptorDir = Core::FileOperations::NovaFileOperations::GetParentDirectory(descriptorPath);
        std::string menuFilePath = Core::FileOperations::NovaFileOperations::JoinPaths(descriptorDir, menuDefinitionFile);
        menuFilePath = Core::FileOperations::NovaFileOperations::NormalizePath(menuFilePath);

        if (!Core::FileOperations::NovaFileOperations::FileExists(menuFilePath)) {
            AddIssue(outIssues,
                     EJsonParseSeverity::Error,
                     descriptor.id + ".canvas.menuDefinitions.file",
                     "Menu definition file does not exist: " + menuFilePath);
            continue;
        }

        const std::string body = Core::FileOperations::NovaFileOperations::ReadTextFile(menuFilePath);
        if (body.empty()) {
            AddIssue(outIssues,
                     EJsonParseSeverity::Error,
                     descriptor.id + ".canvas.menuDefinitions.file",
                     "Menu definition file is empty or unreadable: " + menuFilePath);
            continue;
        }

        nlohmann::json menuJson;
        try {
            menuJson = nlohmann::json::parse(body);
        } catch (const std::exception& ex) {
            AddIssue(outIssues,
                     EJsonParseSeverity::Error,
                     descriptor.id + ".canvas.menuDefinitions.file",
                     "Failed to parse menu definitions JSON: " + std::string(ex.what()));
            continue;
        } catch (...) {
            AddIssue(outIssues,
                     EJsonParseSeverity::Error,
                     menuFilePath,
                     "Failed to parse menu definition JSON due to unknown error.");
            continue;
        }

        std::vector<FJsonParseIssue> parseIssues;
        MenuSchema::FCanvasMenuFile parsedFile;
        const bool parsed = Parser_.Parse(menuJson, parsedFile, parseIssues);
        for (const auto& issue : parseIssues) {
            FJsonParseIssue transformed = issue;
            transformed.JsonPath = menuFilePath + ":" + issue.JsonPath;
            outIssues.push_back(std::move(transformed));
        }

        if (!parsed) {
            continue;
        }

        const std::string ownerExtensionIdFromDescriptor = menuDefinitionsIt->value("ownerExtensionId", descriptor.id);
        if (!ownerExtensionIdFromDescriptor.empty() &&
            parsedFile.OwnerExtensionId != ownerExtensionIdFromDescriptor) {
            AddIssue(outIssues,
                     EJsonParseSeverity::Warning,
                     menuFilePath,
                     "Owner extension mismatch: descriptor declares '" + ownerExtensionIdFromDescriptor +
                         "' but menu file declares '" + parsedFile.OwnerExtensionId + "'.");
        }

        if (parsedFile.OwnerExtensionId.empty()) {
            parsedFile.OwnerExtensionId = ownerExtensionIdFromDescriptor.empty() ? descriptor.id
                                                                                 : ownerExtensionIdFromDescriptor;
        }

        FLoadedMenuDocument document;
        document.SourceExtensionId = descriptor.id;
        document.SourcePath = menuFilePath;
        document.File = std::move(parsedFile);
        BuildRequirementDefinitionDefaultsMap(descriptorJson, document.RequirementDefinitionDefaultsByKey);

        Documents_.push_back(std::move(document));
    }

    for (std::size_t documentIndex = 0; documentIndex < Documents_.size(); ++documentIndex) {
        const auto& document = Documents_[documentIndex];
        for (std::size_t menuIndex = 0; menuIndex < document.File.Menus.size(); ++menuIndex) {
            const auto& menu = document.File.Menus[menuIndex];
            if (menu.Id.empty()) {
                AddIssue(outIssues,
                         EJsonParseSeverity::Error,
                         document.SourcePath,
                         "Encountered menu with empty id.");
                continue;
            }

            const std::string qualifiedId = BuildQualifiedMenuId(document.File.OwnerExtensionId, menu.Id);
            if (QualifiedMenuRefs_.find(qualifiedId) != QualifiedMenuRefs_.end()) {
                AddIssue(outIssues,
                         EJsonParseSeverity::Error,
                         document.SourcePath,
                         "Duplicate qualified menu id found: " + qualifiedId);
                continue;
            }

            FMenuRef menuRef;
            menuRef.DocumentIndex = documentIndex;
            menuRef.MenuIndex = menuIndex;

            QualifiedMenuRefs_.emplace(qualifiedId, menuRef);
            MenuRefsById_[menu.Id].push_back(menuRef);
        }
    }

    return !HasErrorIssues(outIssues);
}

std::vector<std::string> CanvasMenuRuntime::ListMenuIds() const {
    std::vector<std::string> ids;
    ids.reserve(QualifiedMenuRefs_.size());

    for (const auto& entry : QualifiedMenuRefs_) {
        ids.push_back(entry.first);
    }

    std::sort(ids.begin(), ids.end());
    return ids;
}

bool CanvasMenuRuntime::HasMenusForExtension(const std::string& extensionId) const {
    if (extensionId.empty()) {
        return false;
    }

    for (const auto& document : Documents_) {
        const std::string& ownerId = document.File.OwnerExtensionId.empty()
            ? document.SourceExtensionId
            : document.File.OwnerExtensionId;

        if (ownerId == extensionId || document.SourceExtensionId == extensionId) {
            if (!document.File.Menus.empty()) {
                return true;
            }
        }
    }

    return false;
}

std::string CanvasMenuRuntime::GetDefaultMenuIdForExtension(const std::string& extensionId) const {
    if (extensionId.empty()) {
        return "";
    }

    std::string selectedOwnerId;
    std::string selectedMenuId;

    for (const auto& document : Documents_) {
        const std::string ownerId = document.File.OwnerExtensionId.empty()
            ? document.SourceExtensionId
            : document.File.OwnerExtensionId;

        if (ownerId != extensionId && document.SourceExtensionId != extensionId) {
            continue;
        }

        for (const auto& menu : document.File.Menus) {
            if (menu.Id == "main") {
                selectedOwnerId = ownerId;
                selectedMenuId = menu.Id;
                return selectedOwnerId.empty()
                    ? selectedMenuId
                    : selectedOwnerId + "::" + selectedMenuId;
            }
        }

        if (selectedMenuId.empty() && !document.File.Menus.empty()) {
            selectedOwnerId = ownerId;
            selectedMenuId = document.File.Menus.front().Id;
        }
    }

    if (selectedMenuId.empty()) {
        return "";
    }

    return selectedOwnerId.empty() ? selectedMenuId : selectedOwnerId + "::" + selectedMenuId;
}

std::string CanvasMenuRuntime::GetMenuOwnerExtensionId(const std::string& menuId) const {
    FMenuRef menuRef;
    std::string error;
    if (!TryResolveMenuRef(menuId, menuRef, error)) {
        return "";
    }

    if (menuRef.DocumentIndex >= Documents_.size()) {
        return "";
    }

    const auto& document = Documents_[menuRef.DocumentIndex];
    if (!document.File.OwnerExtensionId.empty()) {
        return document.File.OwnerExtensionId;
    }

    return document.SourceExtensionId;
}

bool CanvasMenuRuntime::GetMenuDefinition(const std::string& menuId,
                                          MenuSchema::FCanvasMenuDefinition& outMenu) const {
    FMenuRef menuRef;
    std::string error;
    if (!TryResolveMenuRef(menuId, menuRef, error)) {
        return false;
    }

    const MenuSchema::FCanvasMenuDefinition* menu = FindMenuDefinition(menuRef);
    if (!menu) {
        return false;
    }

    outMenu = *menu;
    return true;
}

MenuSchema::FCanvasRequirementResolveResult CanvasMenuRuntime::ResolveFieldRequirement(
    const std::string& menuId,
    const std::string& fieldId,
    const std::string& consumerExtensionId,
    const std::vector<FCanvasFieldValue>& contextValues) const {
    MenuSchema::FCanvasRequirementResolveResult result;

    FMenuRef menuRef;
    std::string menuResolutionError;
    if (!TryResolveMenuRef(menuId, menuRef, menuResolutionError)) {
        result.Success = false;
        result.ErrorCode = "MenuNotFound";
        result.ErrorMessage = menuResolutionError;
        return result;
    }

    const auto* menu = FindMenuDefinition(menuRef);
    if (!menu) {
        result.Success = false;
        result.ErrorCode = "MenuNotFound";
        result.ErrorMessage = "Menu reference is no longer valid.";
        return result;
    }

    const auto* field = FindFieldDefinition(*menu, fieldId);
    if (!field) {
        result.Success = false;
        result.ErrorCode = "FieldNotFound";
        result.ErrorMessage = "Field '" + fieldId + "' was not found in menu '" + menu->Id + "'.";
        return result;
    }

    if (!field->RequirementBinding.has_value()) {
        result.Success = false;
        result.ErrorCode = "NoRequirementBinding";
        result.ErrorMessage = "Field '" + fieldId + "' does not define a requirementBinding.";
        return result;
    }

    MenuSchema::FCanvasRequirementBinding binding = field->RequirementBinding.value();

    if (menuRef.DocumentIndex < Documents_.size()) {
        const auto& doc = Documents_[menuRef.DocumentIndex];
        auto defaultsIt = doc.RequirementDefinitionDefaultsByKey.find(binding.RequirementKey);
        if (defaultsIt != doc.RequirementDefinitionDefaultsByKey.end()) {
            ApplyRequirementDefaults(&defaultsIt->second, binding);
        }
    }

    if (binding.RequirementKey.empty()) {
        result.Success = false;
        result.ErrorCode = "InvalidRequirementBinding";
        result.ErrorMessage = "Requirement binding key cannot be empty.";
        return result;
    }

    const auto valuesByField = BuildFieldValueMap(contextValues);
    MenuSchema::FCanvasRequirementResolveRequest normalizedRequest;
    normalizedRequest.CorrelationId = "resolve-" + menu->Id + "-" + field->Id;
    normalizedRequest.ConsumerExtensionId = consumerExtensionId;
    normalizedRequest.MenuId = menu->Id;
    normalizedRequest.FieldId = field->Id;
    normalizedRequest.Binding = binding;

    for (const auto& contextBinding : binding.ContextBindings) {
        std::string value;

        if (!contextBinding.FromFieldId.empty()) {
            auto source = valuesByField.find(contextBinding.FromFieldId);
            if (source != valuesByField.end()) {
                value = source->second;
            }
        }

        if (value.empty()) {
            value = contextBinding.FallbackValue;
        }

        if (!contextBinding.RequestKey.empty() && !value.empty()) {
            normalizedRequest.ContextValues.push_back({contextBinding.RequestKey, value});
        }
    }

    const auto descriptors = Core::ExtensionRegistry::Instance().ListExtensionDescriptors();
    std::vector<FResolverCandidate> candidates;
    const std::string collectionKey = ExtractCollectionKey(binding);

    for (const auto& descriptor : descriptors) {
        nlohmann::json descriptorJson = Core::ExtensionDescriptorJson::LoadDescriptorJsonById(descriptor.id);
        if (!descriptorJson.is_object()) {
            continue;
        }

        FResolverCandidate candidate;
        candidate.ExtensionId = descriptor.id;
        candidate.SourceRequirementKey = binding.RequirementKey;
        candidate.SupportsKey = ContainsSupportedKey(descriptorJson, binding.RequirementKey);
        candidate.PreferenceRank = GetPreferenceRank(binding.PreferredResolverExtensionIds, descriptor.id);
        candidate.ExportSymbol = ReadResolverExportSymbol(descriptorJson);
        candidate.DescriptorJson = descriptorJson;

        const auto canvasIt = descriptorJson.find("canvas");
        if (canvasIt != descriptorJson.end() && canvasIt->is_object()) {
            const auto requirementsIt = canvasIt->find("requirements");
            if (requirementsIt != canvasIt->end() && requirementsIt->is_object()) {
                const auto resolverIt = requirementsIt->find("resolver");
                if (resolverIt != requirementsIt->end() && resolverIt->is_object()) {
                    const auto strategyIt = resolverIt->find("descriptorResolutionStrategy");
                    if (strategyIt != resolverIt->end() && strategyIt->is_object()) {
                        const auto authoritativeIt = strategyIt->find("authoritativeRequirementSources");
                        if (authoritativeIt != strategyIt->end() && authoritativeIt->is_object()) {
                            const auto ruleIt = authoritativeIt->find(binding.RequirementKey);
                            if (ruleIt != authoritativeIt->end()) {
                                std::string sourceKey;
                                std::vector<std::string> allowedRequestors;
                                const bool authoritative = ParseAuthoritativeRule(
                                    *ruleIt,
                                    descriptor.id,
                                    binding.RequirementKey,
                                    sourceKey,
                                    allowedRequestors);

                                if (authoritative) {
                                    candidate.Authoritative = true;
                                    candidate.SourceRequirementKey = sourceKey;
                                    candidate.AllowedRequestors = std::move(allowedRequestors);
                                }
                            }
                        }
                    }
                }
            }
        }

        const bool hasDefaultResponses = HasDefaultResponsesForKey(
            descriptorJson,
            candidate.SourceRequirementKey,
            collectionKey);

        if (!candidate.SupportsKey && !candidate.Authoritative && !hasDefaultResponses) {
            continue;
        }

        if (candidate.Authoritative &&
            !IsRequestorAllowed(candidate.AllowedRequestors, normalizedRequest.ConsumerExtensionId)) {
            continue;
        }

        candidates.push_back(std::move(candidate));
    }

    if (candidates.empty()) {
        result.Success = false;
        result.ErrorCode = "ResolverNotFound";
        result.ErrorMessage = "No resolver extension is compatible with requirement key '" +
                              binding.RequirementKey + "'.";
        return result;
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const FResolverCandidate& a, const FResolverCandidate& b) {
        if (a.Authoritative != b.Authoritative) {
            return a.Authoritative && !b.Authoritative;
        }

        if (a.PreferenceRank != b.PreferenceRank) {
            return a.PreferenceRank < b.PreferenceRank;
        }

        if (a.SupportsKey != b.SupportsKey) {
            return a.SupportsKey && !b.SupportsKey;
        }

        return a.ExtensionId < b.ExtensionId;
    });

    std::string lastErrorCode;
    std::string lastErrorMessage;

    for (const auto& candidate : candidates) {
        result.ResolverExtensionId = candidate.ExtensionId;

        if (!candidate.ExportSymbol.empty()) {
            auto& registry = Core::ExtensionRegistry::Instance();
            if (!registry.IsExtensionLoaded(candidate.ExtensionId)) {
                registry.LoadExtensionById(candidate.ExtensionId);
            }

            void* symbol = registry.GetLoadedExtensionSymbol(candidate.ExtensionId, candidate.ExportSymbol);
            if (symbol) {
                using ResolveFn = bool (*)(const void*, void*);
                auto resolveFn = reinterpret_cast<ResolveFn>(symbol);

                Core::RequirementResolver::CoreRequirementResolveRequest cRequest;
                cRequest.RequirementKey = candidate.SourceRequirementKey;
                cRequest.CallerExtensionId = normalizedRequest.ConsumerExtensionId;
                cRequest.SelectedProviderExtensionId = candidate.ExtensionId;
                for (const auto& [contextKey, contextValue] : normalizedRequest.ContextValues) {
                    cRequest.ContextValues.emplace_back(contextKey, contextValue);
                }

                Core::RequirementResolver::CoreRequirementResolveResult cResult;
                const bool invoked = resolveFn(&cRequest, &cResult);
                if (invoked && cResult.Success) {
                    result.Success = true;
                    result.ErrorCode.clear();
                    result.ErrorMessage.clear();
                    result.Options.clear();

                    for (const auto& option : cResult.Options) {
                        MenuSchema::FCanvasResolvedOption converted;
                        converted.Label = option.Label;
                        converted.Value = option.Value;
                        converted.Description = option.Description;
                        result.Options.push_back(std::move(converted));
                    }

                    if (!result.Options.empty()) {
                        return result;
                    }
                }

                lastErrorCode = cResult.ErrorCode.empty() ? "ResolverInvocationFailed" : cResult.ErrorCode;
                lastErrorMessage = cResult.ErrorMessage.empty()
                    ? ("Resolver extension '" + candidate.ExtensionId +
                       "' returned no options for key '" + candidate.SourceRequirementKey + "'.")
                    : cResult.ErrorMessage;
            }
        }

        const auto defaultOptions = Core::ExtensionDescriptorJson::ReadDefaultResolverOptions(
            candidate.DescriptorJson,
            candidate.SourceRequirementKey,
            collectionKey);

        if (!defaultOptions.empty()) {
            result.Success = true;
            result.ErrorCode.clear();
            result.ErrorMessage.clear();
            result.Options.clear();
            result.ResolverExtensionId = candidate.ExtensionId;

            for (const auto& option : defaultOptions) {
                MenuSchema::FCanvasResolvedOption converted;
                converted.Label = option.Label;
                converted.Value = option.Value;
                converted.Description = option.Description;
                result.Options.push_back(std::move(converted));
            }

            return result;
        }
    }

    result.Success = false;
    result.ErrorCode = lastErrorCode.empty() ? "ResolverInvocationFailed" : lastErrorCode;
    result.ErrorMessage = lastErrorMessage.empty()
        ? ("All compatible resolvers failed to resolve key '" + binding.RequirementKey + "'.")
        : lastErrorMessage;
    return result;
}

bool CanvasMenuRuntime::BuildSubmitPayload(const std::string& menuId,
                                           const std::vector<FCanvasFieldValue>& collectedValues,
                                           std::string& outPayloadJson,
                                           std::string& outError) const {
    outPayloadJson.clear();
    outError.clear();

    FMenuRef menuRef;
    if (!TryResolveMenuRef(menuId, menuRef, outError)) {
        return false;
    }

    const auto* menu = FindMenuDefinition(menuRef);
    if (!menu) {
        outError = "Menu reference is no longer valid.";
        return false;
    }

    const auto valuesByField = BuildFieldValueMap(collectedValues);
    nlohmann::json payload = nlohmann::json::object();

    for (const auto& section : menu->Sections) {
        for (const auto& field : section.Fields) {
            if (!field.VisibleIfField.empty()) {
                const auto visibilitySource = valuesByField.find(field.VisibleIfField);
                const std::string resolvedVisibilityValue = visibilitySource == valuesByField.end()
                    ? ""
                    : visibilitySource->second;
                if (resolvedVisibilityValue != field.VisibleIfEquals) {
                    continue;
                }
            }

            std::string fieldValue;
            auto providedValue = valuesByField.find(field.Id);
            if (providedValue != valuesByField.end()) {
                fieldValue = providedValue->second;
            } else {
                fieldValue = field.DefaultValue;
            }

            if (FieldRequiresValue(field) && fieldValue.empty() && field.Type != MenuSchema::ECanvasFieldType::StructObject) {
                outError = "Field '" + field.Id + "' is required but no value was provided.";
                return false;
            }

            if (field.OutputKey.empty()) {
                continue;
            }

            nlohmann::json outputValue;
            bool converted = true;

            switch (field.Type) {
                case MenuSchema::ECanvasFieldType::Bool: {
                    bool resolved = false;
                    if (!TryResolveAsBool(fieldValue, resolved)) {
                        converted = false;
                    } else {
                        outputValue = resolved;
                    }
                    break;
                }
                case MenuSchema::ECanvasFieldType::Integer: {
                    std::int64_t resolved = 0;
                    if (!TryResolveAsInteger(fieldValue, resolved)) {
                        converted = false;
                    } else {
                        outputValue = resolved;
                    }
                    break;
                }
                case MenuSchema::ECanvasFieldType::Float:
                case MenuSchema::ECanvasFieldType::SliderFloat: {
                    double resolved = 0.0;
                    if (!TryResolveAsFloat(fieldValue, resolved)) {
                        converted = false;
                    } else {
                        outputValue = resolved;
                    }
                    break;
                }
                case MenuSchema::ECanvasFieldType::SliderInt: {
                    std::int64_t resolved = 0;
                    if (!TryResolveAsInteger(fieldValue, resolved)) {
                        converted = false;
                    } else {
                        outputValue = resolved;
                    }
                    break;
                }
                case MenuSchema::ECanvasFieldType::StructObject: {
                    if (!fieldValue.empty()) {
                        try {
                            nlohmann::json parsed = nlohmann::json::parse(fieldValue);
                            if (parsed.is_object()) {
                                outputValue = std::move(parsed);
                            }
                        } catch (...) {
                        }
                    }

                    if (outputValue.is_null()) {
                        outputValue = BuildStructObjectValue(field, valuesByField, outError);
                        if (!outError.empty()) {
                            return false;
                        }
                    }
                    break;
                }
                default:
                    outputValue = fieldValue;
                    break;
            }

            if (!converted) {
                outError = "Field '" + field.Id + "' has an invalid value for its type.";
                return false;
            }

            if (!SetJsonPathValue(payload, field.OutputKey, outputValue)) {
                outError = "Field '" + field.Id + "' has invalid outputKey path '" + field.OutputKey + "'.";
                return false;
            }
        }
    }

    outPayloadJson = payload.dump(2);
    return true;
}

bool CanvasMenuRuntime::TryResolveMenuRef(const std::string& menuId,
                                          FMenuRef& outRef,
                                          std::string& outError) const {
    outError.clear();

    if (menuId.empty()) {
        outError = "Menu id cannot be empty.";
        return false;
    }

    const auto qualifiedMatch = QualifiedMenuRefs_.find(menuId);
    if (qualifiedMatch != QualifiedMenuRefs_.end()) {
        outRef = qualifiedMatch->second;
        return true;
    }

    const auto shortMatch = MenuRefsById_.find(menuId);
    if (shortMatch == MenuRefsById_.end()) {
        outError = "No menu found with id '" + menuId + "'.";
        return false;
    }

    if (shortMatch->second.size() > 1) {
        outError = "Menu id '" + menuId +
                   "' is ambiguous. Use a qualified id in the format ownerExtensionId::menuId.";
        return false;
    }

    outRef = shortMatch->second.front();
    return true;
}

const MenuSchema::FCanvasMenuDefinition* CanvasMenuRuntime::FindMenuDefinition(const FMenuRef& ref) const {
    if (ref.DocumentIndex >= Documents_.size()) {
        return nullptr;
    }

    const auto& document = Documents_[ref.DocumentIndex];
    if (ref.MenuIndex >= document.File.Menus.size()) {
        return nullptr;
    }

    return &document.File.Menus[ref.MenuIndex];
}

const MenuSchema::FCanvasFieldDefinition* CanvasMenuRuntime::FindFieldDefinition(
    const MenuSchema::FCanvasMenuDefinition& menu,
    const std::string& fieldId) const {
    for (const auto& section : menu.Sections) {
        for (const auto& field : section.Fields) {
            if (field.Id == fieldId) {
                return &field;
            }
        }
    }

    return nullptr;
}

void CanvasMenuRuntime::BuildRequirementDefinitionDefaultsMap(
    const nlohmann::json& descriptorJson,
    std::unordered_map<std::string, FRequirementDefinitionDefaults>& outDefaults) const {
    outDefaults.clear();

    const auto canvasIt = descriptorJson.find("canvas");
    if (canvasIt == descriptorJson.end() || !canvasIt->is_object()) {
        return;
    }

    const auto requirementsIt = canvasIt->find("requirements");
    if (requirementsIt == canvasIt->end() || !requirementsIt->is_object()) {
        return;
    }

    const auto definitionsIt = requirementsIt->find("definitions");
    if (definitionsIt == requirementsIt->end() || !definitionsIt->is_array()) {
        return;
    }

    for (const auto& definition : *definitionsIt) {
        if (!definition.is_object()) {
            continue;
        }

        const std::string requirementKey = definition.value("key", "");
        if (requirementKey.empty()) {
            continue;
        }

        FRequirementDefinitionDefaults defaults;

        const std::string resolveMode = definition.value("resolveMode", "");
        if (resolveMode == "ValueList") {
            defaults.ResolveMode = MenuSchema::ECanvasRequirementResolveMode::ValueList;
        } else if (resolveMode == "StructuredObject") {
            defaults.ResolveMode = MenuSchema::ECanvasRequirementResolveMode::StructuredObject;
        }

        const std::string strategy = definition.value("strategy", "");
        if (strategy == "ApiRequest") {
            defaults.Strategy = MenuSchema::ECanvasResolverExecutionStrategy::ApiRequest;
        } else if (strategy == "ContentQuery") {
            defaults.Strategy = MenuSchema::ECanvasResolverExecutionStrategy::ContentQuery;
        } else if (strategy == "Custom") {
            defaults.Strategy = MenuSchema::ECanvasResolverExecutionStrategy::Custom;
        }

        defaults.RequestAction = definition.value("requestAction", "");
        defaults.ResponseCollectionPath = definition.value("responseCollectionPath", "");
        defaults.ResponseLabelPath = definition.value("responseLabelPath", "");
        defaults.ResponseValuePath = definition.value("responseValuePath", "");
        defaults.RequestStructName = definition.value("requestStructName", "");
        defaults.OutputStructName = definition.value("outputStructName", "");

        outDefaults[requirementKey] = std::move(defaults);
    }
}

void CanvasMenuRuntime::ApplyRequirementDefaults(const FRequirementDefinitionDefaults* defaults,
                                                 MenuSchema::FCanvasRequirementBinding& binding) const {
    if (!defaults) {
        return;
    }

    if (binding.ResolveMode == MenuSchema::ECanvasRequirementResolveMode::ProviderList &&
        defaults->ResolveMode != MenuSchema::ECanvasRequirementResolveMode::ProviderList) {
        binding.ResolveMode = defaults->ResolveMode;
    }

    if (binding.Strategy == MenuSchema::ECanvasResolverExecutionStrategy::RegistryLookup &&
        defaults->Strategy != MenuSchema::ECanvasResolverExecutionStrategy::RegistryLookup) {
        binding.Strategy = defaults->Strategy;
    }

    if (binding.RequestAction.empty()) {
        binding.RequestAction = defaults->RequestAction;
    }

    if (binding.ResponseCollectionPath.empty()) {
        binding.ResponseCollectionPath = defaults->ResponseCollectionPath;
    }

    if (binding.ResponseLabelPath.empty()) {
        binding.ResponseLabelPath = defaults->ResponseLabelPath;
    }

    if (binding.ResponseValuePath.empty()) {
        binding.ResponseValuePath = defaults->ResponseValuePath;
    }

    if (binding.RequestStructName.empty()) {
        binding.RequestStructName = defaults->RequestStructName;
    }

    if (binding.OutputStructName.empty()) {
        binding.OutputStructName = defaults->OutputStructName;
    }
}

std::string CanvasMenuRuntime::ExtractCollectionKey(const MenuSchema::FCanvasRequirementBinding& binding) const {
    std::string collectionKey = GetLastPathSegment(binding.ResponseCollectionPath);
    if (!collectionKey.empty()) {
        return collectionKey;
    }

    return "environments";
}

} // namespace CanvasCore
