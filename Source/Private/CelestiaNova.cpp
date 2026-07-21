/// @file CelestiaNova.cpp
/// @brief Main entry point for the Celestia Nova application.

#include "NovaCore.h"
#include "NovaMinimal.h"
#include "Core/ExtensionRegistry.h"
#include "Core/FTSTicker.h"
#include "ExtensionSpecific/ICanvasRuntimeSurfaceProvider.h"
#include "Utils/CommandLineParsing.h"
#include "Utils/TerminalUtils.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>
#include "UnitTests/BaseUnitTest.h"
#include "UnitTests/UnitTestManager.h"
#include <cpptrace/cpptrace.hpp>

#ifndef BUILD_CONFIGURATION
#define BUILD_CONFIGURATION "Development"
#endif


using namespace Core;
using namespace Utils;

#include <csignal>

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

    // If PROJECT_SOURCE_DIR is defined at compile time, run from the
    // repository root so Content/ and Extensions/ are resolved relative
    // to the project instead of the binary folder.
#ifdef PROJECT_SOURCE_DIR
    try { std::filesystem::current_path(PROJECT_SOURCE_DIR); } catch(...) {}
#endif
    // Keep the user's terminal dimensions intact. FTXUI needs the real viewport
    // dimensions to align mouse hitboxes with rendered components.

    // Initialize Logging system & Default directories
    NovaLog::StartLogFile();
    NovaLog::CreateRequiredDirectories();

    // Initialize command-line options structure
    CommandLineOptionsStruct cmdOptions;
    {
        NOVA_LOG("Command line arguments received:", LogType::Log);
        for (int i = 0; i < argc; ++i) {
            NOVA_LOG(("Argument " + std::to_string(i) + ": " + argv[i]).c_str(), LogType::Log);
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
        CommandLineParsing::ParseArguments(argc, argv, manager->GetOptionMapping(), manager->GetBoolMapping());
        
        // Execute behaviors for enabled core options
        manager->ExecuteEnabledOptions();
    }

    NOVA_LOG("Starting Celestia Nova", LogType::Log);

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
    Core::ExtensionRegistry::Instance().ApplyCliArguments(argc, argv);

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
