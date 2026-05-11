#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "Core/ModuleAPI.h"

namespace Core {

struct CanvasMenuActionRequest {
    std::string MenuId;
    std::string ActionId;
    std::unordered_map<std::string, std::string> ContextValues;
};

struct CanvasMenuActionResult {
    bool Success = true;
    std::string ErrorMessage;
    // Optional config changes the action wants to write back to global state
    std::unordered_map<std::string, std::string> ConfigUpdates;
    // Optional navigation request
    std::string NavigateToMenuId;
};

// Optional extension interface that allows an extension to receive action callbacks
// from the CanvasCore UI (e.g. ActionButtons or Apply buttons).
class IMenuActionProvider {
public:
    virtual ~IMenuActionProvider() = default;

    virtual CanvasMenuActionResult OnMenuAction(const CanvasMenuActionRequest& request) = 0;
};

} // namespace Core
