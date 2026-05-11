#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Core/IJsonStructParser.h"
#include "ExtensionSpecific/ICanvasRuntimeSurfaceProvider.h"
#include "MenuSchema/CanvasMenuSchema.h"

namespace CanvasCore {

struct FCanvasFieldValue {
    std::string FieldId;
    std::string Value;
};

struct FCanvasMenuChromeState {
    Core::CanvasStatusPillSnapshot StatusPill;
    std::vector<Core::CanvasToastNotification> Toasts;
    std::vector<Core::CanvasPersistentInfoWidget> PersistentInfos;
};

struct FCanvasMenuRenderFrame {
    std::string RequestedMenuId;
    std::string ResolvedMenuId;
    std::string GeneratedAtUtc;
    MenuSchema::FCanvasMenuDefinition Menu;
    FCanvasMenuChromeState Chrome;
};

class ICanvasMenuService {
public:
    virtual ~ICanvasMenuService() = default;

    virtual bool ReloadMenuDefinitions(std::vector<Core::FJsonParseIssue>& outIssues) = 0;
    virtual std::vector<std::string> ListMenuIds() const = 0;

    virtual bool GetMenuDefinition(const std::string& menuId,
                                   MenuSchema::FCanvasMenuDefinition& outMenu) const = 0;

    virtual MenuSchema::FCanvasRequirementResolveResult ResolveFieldRequirement(
        const std::string& menuId,
        const std::string& fieldId,
        const std::string& consumerExtensionId,
        const std::vector<FCanvasFieldValue>& contextValues) const = 0;

    virtual bool BuildSubmitPayload(const std::string& menuId,
                                    const std::vector<FCanvasFieldValue>& collectedValues,
                                    std::string& outPayloadJson,
                                    std::string& outError) const = 0;

    // Compose a renderer-ready frame containing menu definition and shared
    // canvas chrome data (status/toasts/persistent diagnostics).
    virtual bool BuildMenuRenderFrame(const std::string& menuId,
                                      std::size_t maxToasts,
                                      FCanvasMenuRenderFrame& outFrame,
                                      std::string& outError) const = 0;
};

} // namespace CanvasCore
