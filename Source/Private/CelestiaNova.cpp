/// @file CelestiaNova.cpp
/// @brief Main entry point for the Celestia Nova application.

#include "NovaCore.h"
#include "NovaMinimal.h"
#include "Core/ExtensionRegistry.h"
#include "Core/FTSTicker.h"
#include "Core/StatusApiSurface.h"
#include "Core/ProgressTracker.h"
#include "ExtensionSpecific/IExtensionCliProvider.h"
#include "ExtensionSpecific/ICanvasRuntimeSurfaceProvider.h"
#include "Utils/CommandLineParsing.h"
#include "Utils/TerminalUtils.h"
#include <atomic>
#include <array>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <thread>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/screen.hpp>
#include "UnitTests/BaseUnitTest.h"
#include "UnitTests/UnitTestManager.h"
#include <cpptrace/cpptrace.hpp>

#ifndef BUILD_CONFIGURATION
#define BUILD_CONFIGURATION "Development"
#endif


using namespace Core;
using namespace Utils;

#include <csignal>

namespace {

volatile std::sig_atomic_t GServiceStopRequested = 0;

void ServiceStopSignalHandler(int) {
    // SIGTERM/SIGINT are normal service lifecycle events, not crashes.  Keep
    // the handler async-signal-safe and let the main loop perform shutdown.
    GServiceStopRequested = 1;
}

struct ServiceModeOptions {
    bool Enabled = false;
    bool RunOnce = false;
    std::filesystem::path StatusFile = "Runtime/status/service-status.json";
    int StatusIntervalSeconds = 15;
};

enum class CelestCommand { None, Help, Status, Progress, Complete, Interactive };
struct CelestInvocation {
    CelestCommand command = CelestCommand::None;
    std::string completionPrefix;
    std::vector<std::string> translatedArguments;
};

// A double-clicked Windows executable starts with Binaries as its working
// directory.  Extensions and Content live one level above it, while a Linux
// package starts from /opt/celestianova.  Resolve that runtime root from the
// executable location before any registry or logging work begins.
void EnsureRuntimeWorkingDirectory(const char* executablePath) {
    std::error_code error;
    const auto executable = std::filesystem::absolute(
        executablePath ? std::filesystem::path(executablePath) : std::filesystem::path{}, error);
    if (!error && !executable.empty()) {
        const auto binaryDirectory = executable.parent_path();
        const std::array<std::filesystem::path, 2> candidates{
            binaryDirectory,
            binaryDirectory.parent_path()
        };
        for (const auto& candidate : candidates) {
            error.clear();
            if (std::filesystem::is_directory(candidate / "Extensions", error)) {
                std::filesystem::current_path(candidate, error);
                return;
            }
        }
    }

    // Only use the caller's working directory when the executable path could
    // not identify a package root. This fallback keeps embedded/test launches
    // functional without letting a source checkout hijack a direct run of a
    // packaged GUI binary.
    error.clear();
    const auto current = std::filesystem::current_path(error);
    if (!error && std::filesystem::is_directory(current / "Extensions", error)) return;
}

CelestInvocation ParseCelestInvocation(int argc, const char* argv[]) {
    CelestInvocation invocation;
    for (int index = 1; index < argc; ++index) invocation.translatedArguments.emplace_back(argv[index] ? argv[index] : "");
    if (invocation.translatedArguments.empty() || invocation.translatedArguments.front() != "--celest") return invocation;
    invocation.translatedArguments.erase(invocation.translatedArguments.begin());
    if (invocation.translatedArguments.empty()) { invocation.command = CelestCommand::Interactive; return invocation; }
    const auto command = invocation.translatedArguments.front();
    invocation.translatedArguments.erase(invocation.translatedArguments.begin());
    if (command == "help") { invocation.command = CelestCommand::Help; return invocation; }
    if (command == "status") { invocation.command = CelestCommand::Status; return invocation; }
    if (command == "progress") { invocation.command = CelestCommand::Progress; return invocation; }
    if (command == "complete") { invocation.command = CelestCommand::Complete; if (!invocation.translatedArguments.empty()) invocation.completionPrefix = invocation.translatedArguments.front(); return invocation; }
    if (command == "deploy" || command == "start") {
        if (!invocation.translatedArguments.empty()) {
            const auto content = invocation.translatedArguments.front();
            const auto depth = invocation.translatedArguments.size() > 1 ? invocation.translatedArguments[1] : "auto";
            invocation.translatedArguments = {"--run-once", "--deploy-local-content", content, "--configuration-depth", depth};
        }
        return invocation;
    }
    if (command == "stop" || command == "logs") {
        if (!invocation.translatedArguments.empty()) {
            invocation.translatedArguments = {"--run-once", command == "stop" ? "--stop-local-content" : "--local-content-logs", invocation.translatedArguments.front()};
        }
        return invocation;
    }
    // "celest run --extension-flag value" is the universal escape hatch for
    // every extension descriptor without a wrapper release.
    if (command == "run") { invocation.translatedArguments.insert(invocation.translatedArguments.begin(), "--run-once"); return invocation; }
    invocation.command = CelestCommand::Help;
    return invocation;
}

void RenderCelestProgress() {
    auto screen = ftxui::ScreenInteractive::Fullscreen();
    std::atomic<bool> monitoring{true};
    std::thread refresh([&] {
        while (monitoring.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            screen.PostEvent(ftxui::Event::Custom);
        }
    });
    auto view = ftxui::Renderer([&] {
        const auto progress = Core::ProgressTracker::Read();
        const auto state = progress.active ? "running" : (progress.failed ? "failed" : "completed");
        const std::array<std::string, 4> spinnerFrames{"⠋", "⠙", "⠹", "⠸"};
        const auto tick = static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() / 250);
        const auto spinner = progress.active ? spinnerFrames[tick % spinnerFrames.size()] : "•";
        ftxui::Elements activity;
        for (const auto& entry : Core::ProgressTracker::ReadRecentActivity()) {
            activity.push_back(ftxui::text("  " + entry) | ftxui::dim);
        }
        if (activity.empty()) activity.push_back(ftxui::text("  Waiting for the first reported operation.") | ftxui::dim);
        return ftxui::vbox({
            ftxui::text(" CELESTIA NOVA / LIVE PROGRESS ") | ftxui::bold | ftxui::color(ftxui::Color::Cyan),
            ftxui::separator(),
            ftxui::gauge(progress.percent / 100.0f) | ftxui::color(progress.failed ? ftxui::Color::Red : ftxui::Color::Green),
            ftxui::text(spinner + "  " + std::to_string(progress.percent) + "%  " + state),
            ftxui::text(progress.owner + ": " + progress.phase),
            ftxui::separator(),
            ftxui::text("Recent operation activity") | ftxui::bold,
            ftxui::vbox(std::move(activity)) | ftxui::border,
            ftxui::text("Updates every 250 ms  •  Esc exits") | ftxui::dim,
        }) | ftxui::border;
    });
    auto interactive = ftxui::CatchEvent(view, [&](ftxui::Event event) {
        if (event == ftxui::Event::Escape) { screen.Exit(); return true; }
        return false;
    });
    screen.Loop(interactive);
    monitoring.store(false, std::memory_order_relaxed);
    refresh.join();
}

void PrintCelestHelp() {
    std::cout << "celest help | status | progress | complete <prefix>\n"
              << "celest deploy <content-id> [auto|minimal|normal|advanced]\n"
              << "celest start|stop|logs <content-id>\n"
              << "celest run --<extension-command> [value]\n";
}

std::vector<std::string> CelestSuggestions(const std::string& prefix) {
    const std::vector<std::string> builtins{"help", "status", "progress", "complete", "deploy", "start", "stop", "logs", "run", "exit"};
    std::vector<std::string> suggestions;
    for (const auto& item : builtins) {
        if (item.rfind(prefix, 0) == 0) suggestions.push_back(item);
    }
    for (const auto& descriptor : Core::ExtensionRegistry::Instance().ListExtensionDescriptors()) {
        auto* provider = dynamic_cast<Core::IExtensionCliProvider*>(
            Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance(descriptor.id));
        if (!provider) continue;
        for (const auto& command : provider->GetCliArgDescriptors()) {
            const std::string flag = "--" + command.Flag;
            if (flag.rfind(prefix, 0) == 0) suggestions.push_back(flag);
        }
    }
    return suggestions;
}

std::vector<std::string> SplitCelestLine(const std::string& line) {
    std::istringstream stream(line);
    std::vector<std::string> parts;
    std::string part;
    while (stream >> part) parts.push_back(part);
    return parts;
}

void RunInteractiveCelestConsole() {
    using namespace ftxui;
    std::string commandLine;
    std::string transcript = "Ready. Type a command; Tab accepts the first suggestion; Esc exits.";
    std::vector<std::string> suggestions;
    auto screen = ScreenInteractive::Fullscreen();
    std::atomic<bool> consoleRunning{true};
    std::thread progressRefresh([&] {
        while (consoleRunning.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            screen.PostEvent(Event::Custom);
        }
    });
    auto input = Input(&commandLine, "command");

    auto refreshSuggestions = [&]() {
        const auto parts = SplitCelestLine(commandLine);
        suggestions = CelestSuggestions(parts.empty() ? commandLine : parts.front());
        if (suggestions.size() > 6) suggestions.resize(6);
    };
    refreshSuggestions();

    auto renderer = Renderer(input, [&] {
        Elements matches;
        if (suggestions.empty()) matches.push_back(text("no matching command"));
        else for (const auto& suggestion : suggestions) matches.push_back(text("  " + suggestion));
        const auto progress = Core::ProgressTracker::Read();
        const int filled = std::max(0, std::min(24, progress.percent * 24 / 100));
        return vbox({
            text(" CELESTIA NOVA / CELEST COMMAND CONSOLE ") | bold | color(Color::Cyan),
            separator(),
            hbox(text("node  ") | bold, text("local daemon command surface")),
            hbox(text("work  ") | bold, text(progress.owner.empty() ? "idle" : progress.owner + " — " + progress.phase)),
            gauge(progress.percent / 100.0f) | color(progress.active ? Color::Green : Color::GrayLight),
            separator(),
            text(transcript) | flex,
            separator(),
            hbox(text("celest> ") | bold | color(Color::Green), input->Render()),
            text("suggestions") | dim,
            vbox(std::move(matches)) | border | color(Color::GrayLight),
            text("Enter executes  •  Tab completes  •  Esc exits") | dim,
        }) | border | flex;
    });

    auto interactive = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Escape) {
            screen.Exit();
            return true;
        }
        if (event == Event::Tab && !suggestions.empty()) {
            commandLine = suggestions.front();
            refreshSuggestions();
            return true;
        }
        if (event == Event::Return) {
            const auto parts = SplitCelestLine(commandLine);
            if (parts.empty()) return true;
            if (parts.front() == "exit" || parts.front() == "quit") {
                screen.Exit();
                return true;
            }
            if (parts.front() == "help") {
                transcript = "Built-ins: status, progress, deploy, start, stop, logs, run. Extension flags are listed below as you type.";
            } else if (parts.front() == "status") {
                transcript = Core::StatusApiSurface::BuildExtensionsStatusJson();
            } else if (parts.front() == "progress") {
                const auto progress = Core::ProgressTracker::Read();
                transcript = progress.owner + ": " + progress.phase + " (" + std::to_string(progress.percent) + "%)";
            } else {
                std::vector<std::string> argvStorage{"celest", "--celest"};
                argvStorage.insert(argvStorage.end(), parts.begin(), parts.end());
                std::vector<const char*> argv;
                for (const auto& item : argvStorage) argv.push_back(item.c_str());
                const CelestInvocation invocation = ParseCelestInvocation(static_cast<int>(argv.size()), argv.data());
                if (invocation.command == CelestCommand::Help) {
                    transcript = "Unknown command. Type help or use a listed extension flag through: run --flag value";
                } else {
                    std::vector<const char*> effective;
                    effective.push_back(argvStorage.front().c_str());
                    for (const auto& item : invocation.translatedArguments) effective.push_back(item.c_str());
                    Core::ProgressTracker::Publish({"interactive-cli", "celest", "Dispatching " + commandLine, 10, true});
                    Core::ExtensionRegistry::Instance().ApplyCliArguments(static_cast<int>(effective.size()), effective.data());
                    Core::ProgressTracker::Publish({"interactive-cli", "celest", "Command dispatch complete", 100, false});
                    transcript = "Dispatched: " + commandLine;
                }
            }
            commandLine.clear();
            refreshSuggestions();
            return true;
        }
        const bool handled = input->OnEvent(event);
        if (handled) refreshSuggestions();
        return handled;
    });
    screen.Loop(interactive);
    consoleRunning.store(false, std::memory_order_relaxed);
    progressRefresh.join();
}

ServiceModeOptions ParseServiceModeOptions(int argc, const char* argv[]) {
    ServiceModeOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index] ? argv[index] : "";
        if (argument == "--service-mode" || argument == "--daemon") {
            options.Enabled = true;
        } else if (argument == "--run-once") {
            options.RunOnce = true;
        } else if (argument == "--status-file" && index + 1 < argc) {
            options.StatusFile = argv[++index];
        } else if (argument == "--status-interval-seconds" && index + 1 < argc) {
            try {
                options.StatusIntervalSeconds = std::stoi(argv[++index]);
            } catch (...) {
                options.StatusIntervalSeconds = 15;
            }
        }
    }
    options.StatusIntervalSeconds = std::max(5, std::min(options.StatusIntervalSeconds, 3600));
    return options;
}

bool WriteServiceStatusSnapshot(const std::filesystem::path& statusFile) {
    try {
        std::filesystem::create_directories(statusFile.parent_path());
        const auto temporaryFile = statusFile.string() + ".tmp";
        std::ofstream output(temporaryFile, std::ios::out | std::ios::trunc);
        if (!output) {
            return false;
        }
        output << Core::StatusApiSurface::BuildExtensionsStatusJson();
        output << '\n';
        output.close();
        std::filesystem::rename(temporaryFile, statusFile);
        std::error_code permissionError;
        std::filesystem::permissions(statusFile.parent_path(),
            std::filesystem::perms::owner_all |
            std::filesystem::perms::group_read | std::filesystem::perms::group_exec |
            std::filesystem::perms::others_read | std::filesystem::perms::others_exec,
            std::filesystem::perm_options::replace, permissionError);
        std::filesystem::permissions(statusFile,
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
            std::filesystem::perms::group_read | std::filesystem::perms::others_read,
            std::filesystem::perm_options::replace, permissionError);
        return true;
    } catch (const std::exception& ex) {
        NOVA_LOG((std::string("Service-mode status write failed: ") + ex.what()).c_str(), LogType::Warning);
        return false;
    }
}

int RunServiceMode(const ServiceModeOptions& options) {
    NOVA_LOG(("Service mode started; reporting status to " + options.StatusFile.string()).c_str(), LogType::Log);
    std::signal(SIGTERM, ServiceStopSignalHandler);
    std::signal(SIGINT, ServiceStopSignalHandler);

    auto nextStatusWrite = std::chrono::steady_clock::now();
    auto lastTick = std::chrono::steady_clock::now();
    while (!GServiceStopRequested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const auto now = std::chrono::steady_clock::now();
        Core::FTSTicker::GetCoreTicker().Tick(std::chrono::duration<float>(now - lastTick).count());
        lastTick = now;

        if (now >= nextStatusWrite) {
            WriteServiceStatusSnapshot(options.StatusFile);
            nextStatusWrite = now + std::chrono::seconds(options.StatusIntervalSeconds);
        }
    }

    // Always leave a final snapshot so the API/dashboard can distinguish a
    // clean stop from a stale heartbeat.
    WriteServiceStatusSnapshot(options.StatusFile);
    NOVA_LOG("Service mode stopped cleanly", LogType::Log);
    return 0;
}

} // namespace

void SignalHandler(int signum)
{
    // A fatal signal was caught (e.g. SIGSEGV/segfault). Auto-wire to Nova.log
    std::string context = "Fatal Signal Caught: " + std::to_string(signum);    
    Core::NovaLog::LogObjectTrace(context.c_str());
    
    // We then abort to terminate the stuck process
    std::abort();
}

void RegisterCrashHandler()
{
    // Auto-capture stack traces for unhandled C++ exceptions directly to NovaLog
    std::set_terminate([]() {
        Core::NovaLog::LogStackTrace("Unhandled Exception");
        std::abort();
    });

    // Auto-capture deep signal-safe object traces for severe hardware faults to NovaLog
    std::signal(SIGSEGV, SignalHandler); // Segmentation fault
    std::signal(SIGABRT, SignalHandler); // Abort
    std::signal(SIGILL,  SignalHandler); // Illegal instruction
    std::signal(SIGFPE,  SignalHandler); // Floating point exception
}



int main(int argc, const char* argv[])
{
    RegisterCrashHandler();
    EnsureRuntimeWorkingDirectory(argc > 0 ? argv[0] : nullptr);

    const CelestInvocation celestInvocation = ParseCelestInvocation(argc, argv);
    std::vector<const char*> effectiveArgv;
    effectiveArgv.reserve(celestInvocation.translatedArguments.size() + 1);
    effectiveArgv.push_back(argv[0]);
    for (const auto& argument : celestInvocation.translatedArguments) effectiveArgv.push_back(argument.c_str());
    const int effectiveArgc = static_cast<int>(effectiveArgv.size());

    // The launcher/service controls its working directory.  In particular a
    // packaged Production binary must remain under /opt/celestianova instead
    // of jumping back to the build machine's source checkout.
    // Keep the user's terminal dimensions intact. FTXUI needs the real viewport
    // dimensions to align mouse hitboxes with rendered components.

    const ServiceModeOptions serviceModeOptions = ParseServiceModeOptions(effectiveArgc, effectiveArgv.data());

    // Initialize Logging system & Default directories
    NovaLog::StartLogFile();
    NovaLog::CreateRequiredDirectories();

    // Initialize command-line options structure
    CommandLineOptionsStruct cmdOptions;
    {
        NOVA_LOG("Command line arguments received:", LogType::Log);
        for (int i = 0; i < effectiveArgc; ++i) {
            NOVA_LOG(("Argument " + std::to_string(i) + ": " + effectiveArgv[i]).c_str(), LogType::Log);
        }

        // Register Core command-line options dynamically
        auto* manager = CommandLineOptions::GetSingletonInstance();
        manager->RegisterBoolOption("verbose", &cmdOptions.verbose, []() {
            NovaLog::SetVerbose(true);
            NOVA_LOG("Verbose mode enabled via CLI", LogType::Log);
        });
        manager->RegisterBoolOption("no-root", &cmdOptions.noRoot, []() {
            NOVA_LOG("No-root mode enabled via CLI", LogType::Log);
        });
        manager->RegisterBoolOption("clear-content", &cmdOptions.clearContent, []() {
            NOVA_LOG("Content clearing requested via CLI", LogType::Log);
        });
        manager->RegisterOption("mkdocs-path", &cmdOptions.mkdocsProjectPath, []() {
            NOVA_LOG("MkDocs path set via CLI", LogType::Log);
        });

        // Parse command-line arguments for core options
        CommandLineParsing::ParseArguments(effectiveArgc, effectiveArgv.data(), manager->GetOptionMapping(), manager->GetBoolMapping());
        
        // Execute behaviors for enabled core options
        manager->ExecuteEnabledOptions();
    }

    NOVA_LOG("Starting Celestia Nova", LogType::Log);

    // Progress is a persisted daemon snapshot and needs neither plug-in
    // discovery nor startup.  Enter its live FTXUI monitor immediately.
    if (celestInvocation.command == CelestCommand::Progress) {
        RenderCelestProgress();
        return 0;
    }

    // CanvasCore is expected to autostart and provide app menu definitions.
    Core::InitializeExtensions("Extensions");
    {
        const int kWaitSliceMs = 25;
        const int kMaxWaitMs = 2000;
        int waitedMs = 0;
        while (waitedMs < kMaxWaitMs && !Core::ExtensionRegistry::Instance().IsExtensionLoaded("canvascore")) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kWaitSliceMs));
            waitedMs += kWaitSliceMs;
        }

        if (Core::ExtensionRegistry::Instance().IsExtensionLoaded("canvascore")) {
            NOVA_LOG("CanvasCore loaded during startup. Menu definitions are available.", LogType::Log);
        } else {
            NOVA_LOG("CanvasCore was not loaded within startup wait window.", LogType::Warning);
        }
    }

    // Dispatch CLI arguments to all loaded extensions
    if (celestInvocation.command == CelestCommand::Interactive) {
        RunInteractiveCelestConsole();
        return 0;
    }
    if (celestInvocation.command == CelestCommand::Help) {
        PrintCelestHelp();
        return 0;
    }
    if (celestInvocation.command == CelestCommand::Status) {
        std::cout << Core::StatusApiSurface::BuildExtensionsStatusJson() << '\n';
        return 0;
    }
    if (celestInvocation.command == CelestCommand::Complete) {
        for (const auto& item : CelestSuggestions(celestInvocation.completionPrefix)) std::cout << item << '\n';
        return 0;
    }

    Core::ProgressTracker::Publish({"cli-dispatch", "celest", "Dispatching extension command", 10, true});
    Core::ExtensionRegistry::Instance().ApplyCliArguments(effectiveArgc, effectiveArgv.data());
    const auto extensionProgress = Core::ProgressTracker::Read();
    if (extensionProgress.owner == "celest") {
        Core::ProgressTracker::Publish({"cli-dispatch", "celest", "Command dispatch complete", 100, false});
    }

    // A deployment unit performs all of its work during CLI dispatch.  Unlike
    // the long-running status daemon it must return a truthful systemd result
    // once those synchronous orchestration actions have completed.
    if (serviceModeOptions.RunOnce) {
        WriteServiceStatusSnapshot(serviceModeOptions.StatusFile);
        return Core::ProgressTracker::Read().failed ? 1 : 0;
    }

    const std::string buildConfiguration = BUILD_CONFIGURATION;

    if (buildConfiguration == "Development" || buildConfiguration == "Testing")
    {
        NOVA_LOG("Running in Development or Testing mode", LogType::Warning);
        NovaLog::SetVerbose(true);

        NOVA_LOG("Checking for unit tests...", LogType::Log);

        // Initialize unit test 
        UnitTestManager unitTestManager;
        unitTestManager.Initialize(argc, argv);

        // Run unit tests if requested
        if (unitTestManager.RunUnitTests()) {
            return 0; 
        }
    }
    else if(buildConfiguration == "Production")
    {
        NOVA_LOG("Running in Production mode", LogType::Log);
        NovaLog::SetVerbose(false); 
    }

    

    if (serviceModeOptions.Enabled) {
        return RunServiceMode(serviceModeOptions);
    }

    auto* canvasRuntimeSurface = dynamic_cast<Core::ICanvasRuntimeSurfaceProvider*>(
        Core::ExtensionRegistry::Instance().GetLoadedExtensionInstance("canvascore"));
    if (!canvasRuntimeSurface) {
        NOVA_LOG("CanvasCore runtime surface is unavailable. Application cannot launch canvas menus.", LogType::Error);
        return 1;
    }

    // Drive FTSTicker on a background thread so periodic extension work
    // does not block the canvas menu loop.
    std::atomic<bool> tickerRunning{true};
    std::thread tickerThread([&tickerRunning]() {
        using Clock = std::chrono::steady_clock;
        auto last = Clock::now();
        while (tickerRunning.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 Hz
            const auto now = Clock::now();
            const float deltaSeconds = std::chrono::duration<float>(now - last).count();
            last = now;
            Core::FTSTicker::GetCoreTicker().Tick(deltaSeconds);
        }
    });

    std::string canvasRenderError;
    if (!canvasRuntimeSurface->RunCanvasMenuLoop("main", canvasRenderError)) {
        const std::string logMessage = canvasRenderError.empty()
            ? std::string("CanvasCore failed to render the menu loop.")
            : std::string("CanvasCore failed to render the menu loop: ") + canvasRenderError;
        NOVA_LOG(logMessage.c_str(), LogType::Error);
        tickerRunning.store(false);
        tickerThread.join();
        return 1;
    }

    tickerRunning.store(false);
    tickerThread.join();
    return 0;
}
