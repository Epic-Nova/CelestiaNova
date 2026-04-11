#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "CanvasMenuService.h"
#include "MenuSchema/CanvasMenuJsonParser.h"

namespace CanvasCore {

class CanvasMenuRuntime {
public:
    bool ReloadMenuDefinitions(std::vector<Core::FJsonParseIssue>& outIssues);

    std::vector<std::string> ListMenuIds() const;

    bool GetMenuDefinition(const std::string& menuId,
                           MenuSchema::FCanvasMenuDefinition& outMenu) const;

    MenuSchema::FCanvasRequirementResolveResult ResolveFieldRequirement(
        const std::string& menuId,
        const std::string& fieldId,
        const std::string& consumerExtensionId,
        const std::vector<FCanvasFieldValue>& contextValues) const;

    bool BuildSubmitPayload(const std::string& menuId,
                            const std::vector<FCanvasFieldValue>& collectedValues,
                            std::string& outPayloadJson,
                            std::string& outError) const;

private:
    struct FRequirementDefinitionDefaults {
        MenuSchema::ECanvasRequirementResolveMode ResolveMode = MenuSchema::ECanvasRequirementResolveMode::ProviderList;
        MenuSchema::ECanvasResolverExecutionStrategy Strategy = MenuSchema::ECanvasResolverExecutionStrategy::RegistryLookup;
        std::string RequestAction;
        std::string ResponseCollectionPath;
        std::string ResponseLabelPath;
        std::string ResponseValuePath;
        std::string RequestStructName;
        std::string OutputStructName;
    };

    struct FLoadedMenuDocument {
        std::string SourceExtensionId;
        std::string SourcePath;
        MenuSchema::FCanvasMenuFile File;
        std::unordered_map<std::string, FRequirementDefinitionDefaults> RequirementDefinitionDefaultsByKey;
    };

    struct FMenuRef {
        std::size_t DocumentIndex = 0;
        std::size_t MenuIndex = 0;
    };

    bool TryResolveMenuRef(const std::string& menuId, FMenuRef& outRef, std::string& outError) const;

    const MenuSchema::FCanvasMenuDefinition* FindMenuDefinition(const FMenuRef& ref) const;

    const MenuSchema::FCanvasFieldDefinition* FindFieldDefinition(
        const MenuSchema::FCanvasMenuDefinition& menu,
        const std::string& fieldId) const;

    void BuildRequirementDefinitionDefaultsMap(
        const nlohmann::json& descriptorJson,
        std::unordered_map<std::string, FRequirementDefinitionDefaults>& outDefaults) const;

    void ApplyRequirementDefaults(const FRequirementDefinitionDefaults* defaults,
                                  MenuSchema::FCanvasRequirementBinding& binding) const;

    std::string ExtractCollectionKey(const MenuSchema::FCanvasRequirementBinding& binding) const;

    std::vector<FLoadedMenuDocument> Documents_;
    std::unordered_map<std::string, FMenuRef> QualifiedMenuRefs_;
    std::unordered_map<std::string, std::vector<FMenuRef>> MenuRefsById_;
    MenuSchema::CanvasMenuJsonParser Parser_;
};

} // namespace CanvasCore
