#include "UnattendedModeManager.h"
#include <iomanip>
#include <sstream>
#include <ctime>

namespace CanvasCore {

UnattendedModeManager::UnattendedModeManager() {
    Reset();
}

void UnattendedModeManager::Reset() {
    State = FUnattendedModeState();
}

void UnattendedModeManager::Trigger(const std::string& scenario) {
    State.bActive = true;
    State.Progress = 0.0f;
    State.Logs.clear();
    State.CurrentStep = "STARTING";
    State.TestScenario = scenario;
    State.LastLogProgress = 0.0f;
    
    FLogEntry startLog;
    startLog.Id = "SYS_INIT";
    startLog.Timestamp = "02:14:55";
    startLog.Message = "Launching unattended setup [" + State.TestScenario + "] from payload.";
    startLog.Severity = Core::CanvasNotificationSeverity::Info;
    State.Logs.push_back(startLog);
}

void UnattendedModeManager::Update() {
    if (!State.bActive || State.Progress >= 1.0f) return;

    State.Progress += 0.003f;
    
    if (State.Progress >= State.LastLogProgress + 0.12f) {
        State.LastLogProgress = State.Progress;
        
        FLogEntry entry;
        entry.Id = "TASK_" + std::to_string((int)(State.Progress * 100));
        entry.Timestamp = "02:15:22"; // Mock timestamp
        
        if (State.Progress < 0.3f) {
            State.CurrentStep = "PROVISIONING_RESOURCES";
            entry.Message = "Allocating virtual mesh nodes in region: EU-WEST";
            entry.Severity = Core::CanvasNotificationSeverity::Info;
        } else if (State.Progress < 0.6f) {
            State.CurrentStep = "SYNCING_ORCHESTRATORS";
            entry.Message = "Synchronizing state with DockerOrchestrator... [OK]";
            entry.Severity = Core::CanvasNotificationSeverity::Success;
        } else if (State.Progress < 0.9f) {
            State.CurrentStep = "DEPLOYING_CAPABILITIES";
            entry.Message = "Warning: latency detected in SignalCore bridge.";
            entry.Severity = Core::CanvasNotificationSeverity::Warning;
        } else {
            State.CurrentStep = "FINALIZING_SETUP";
            if (State.TestScenario == "FAIL" && State.Progress > 0.92f) {
                entry.Message = "CRITICAL: Global consistency check FAILED. ID_774";
                entry.Severity = Core::CanvasNotificationSeverity::Error;
            } else {
                entry.Message = "Verifying global connectivity... Success.";
                entry.Severity = Core::CanvasNotificationSeverity::Info;
            }
        }
        State.Logs.push_back(entry);
    }

    if (State.Progress >= 1.0f) {
        State.Progress = 1.0f;
        State.CurrentStep = (State.TestScenario == "FAIL") ? "FAILED" : "COMPLETED";
        
        FLogEntry finalLog;
        finalLog.Id = (State.TestScenario == "FAIL") ? "SYS_ABORT" : "SYS_READY";
        finalLog.Timestamp = "02:16:05";
        finalLog.Message = (State.TestScenario == "FAIL") 
            ? "Unattended operation ABORTED due to critical errors." 
            : "Unattended operation completed successfully. System NOMINAL.";
        finalLog.Severity = (State.TestScenario == "FAIL") 
            ? Core::CanvasNotificationSeverity::Error 
            : Core::CanvasNotificationSeverity::Success;
        State.Logs.push_back(finalLog);
    }
}

ftxui::Element UnattendedModeManager::RenderUI(const ftxui::Element& header, 
                                             std::function<ftxui::Color(int)> getRainbowColor) {
    using namespace ftxui;

    auto loadingBar = gauge(State.Progress) | border | color(getRainbowColor(180));
    
    Elements logLines;
    for (const auto& log : State.Logs) {
        Color logColor = Color::White;
        if (log.Severity == Core::CanvasNotificationSeverity::Warning) logColor = Color::Yellow;
        else if (log.Severity == Core::CanvasNotificationSeverity::Error) logColor = Color::Red;
        else if (log.Severity == Core::CanvasNotificationSeverity::Success) logColor = Color::Green;
        
        logLines.push_back(hbox({
            text("[" + log.Timestamp + "] ") | color(Color::GrayDark),
            text("[" + log.Id + "] ") | color(Color::Cyan),
            paragraph(log.Message) | color(logColor)
        }));
    }

    auto logWindow = vbox(std::move(logLines)) | vscroll_indicator | frame | size(HEIGHT, EQUAL, 12) | borderStyled(BorderStyle::LIGHT);
    
    auto resumeButton = Button(" [ RESUME AUTOSCROLL ] ", [&] {
        State.bAutoscroll = true;
        State.bManualScrollDetected = false;
    }, ButtonOption::Simple()) | color(Color::Green);

    auto unattendedUI = vbox({
        header,
        vbox({
            text(" UNATTENDED OPERATION IN PROGRESS ") | center | bold | color(getRainbowColor(0)),
            separatorEmpty(),
            hbox({ text(" STEP: "), text(State.CurrentStep) | color(Color::Cyan) }) | center,
            loadingBar | size(WIDTH, EQUAL, 60) | center,
            separatorLight(),
            text(" DIAGNOSTIC LOG ") | bold,
            logWindow,
            State.bManualScrollDetected ? (resumeButton->Render() | center) : text("")
        }) | flex | border | color(getRainbowColor(90))
    }) | size(WIDTH, EQUAL, 110);

    return unattendedUI;
}

} // namespace CanvasCore
