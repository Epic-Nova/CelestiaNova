#pragma once

#include <string>
#include <vector>
#include <functional>
#include "ExtensionSpecific/ICanvasRuntimeSurfaceProvider.h"
#include "ftxui/dom/elements.hpp"
#include "ftxui/component/component.hpp"

namespace CanvasCore {

struct FLogEntry {
    std::string Id;
    std::string Message;
    Core::CanvasNotificationSeverity Severity;
    std::string Timestamp;
};

struct FUnattendedModeState {
    bool bActive = false;
    std::string CurrentStep = "IDLE";
    float Progress = 0.0f;
    std::vector<FLogEntry> Logs;
    bool bAutoscroll = true;
    int ScrollPosition = 0;
    int MaxScrollPosition = 0;
    bool bManualScrollDetected = false;
    std::string TestScenario = "SUCCESS";
    float LastLogProgress = 0.0f;
};

class UnattendedModeManager {
public:
    UnattendedModeManager();

    void Trigger(const std::string& scenario);
    void Update();
    void Reset();

    bool IsActive() const { return State.bActive; }
    
    ftxui::Element RenderUI(const ftxui::Element& header, 
                           std::function<ftxui::Color(int)> getRainbowColor);

private:
    FUnattendedModeState State;
};

} // namespace CanvasCore
