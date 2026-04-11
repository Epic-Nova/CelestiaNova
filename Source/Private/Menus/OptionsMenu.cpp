#include "NovaCore.h"
#include "NovaMinimal.h"
#include "Utils/ScrollableLog.h"

//@TODO: Implement the options from the struct in the functionality of the program
namespace Menus
{
    using namespace ftxui;
    using namespace Core;

    OptionsMenu* OptionsMenu::SingletonPtr = nullptr;
    
    std::shared_ptr<OptionsMenu> OptionsMenu::Create(CommandLineOptionsStruct& options)
    {
        auto instance = std::shared_ptr<OptionsMenu>(new OptionsMenu(options));
        instance->LoadConfig(); // Load configuration upon creation
        SingletonPtr = instance.get(); // Set the singleton pointer
        return instance;
    }

    void OptionsMenu::LoadConfig()
    {
        NOVA_LOG("Loading configuration...", LogType::Log);
        try {
            std::string configPath = std::filesystem::current_path().string() + "/Configs/app_config.json";
            //Log the path for debugging
            if (std::filesystem::exists(configPath)) {
                std::ifstream configFile(configPath);
                nlohmann::json configJson;
                configFile >> configJson;

                options_.clearContent = configJson.value("clearContent", false);
                options_.noRoot = configJson.value("noRoot", false);
                options_.verbose = configJson.value("verbose", false);
                options_.requestRootForBrew = configJson.value("requestRootForBrew", true);
                options_.requestRootForPip = configJson.value("requestRootForPip", true);
                options_.requestRootForVenv = configJson.value("requestRootForVenv", true);
                options_.scrollableLogAlwaysVisible = configJson.value("scrollableLogAlwaysVisible", false);
                options_.disableMousePartyMode = configJson.value("disableMousePartyMode", false);
                options_.mkdocsProjectPath = configJson.value("mkdocsProjectPath", "I am a Diosaur, rawr!");

                NOVA_LOG("Configuration loaded successfully.", LogType::Log);
            } else {
                NOVA_LOG("Configuration file not found, recreating default configuration.", LogType::Warning);
                SaveConfig(); // Save default config if file doesn't exist
            }
        } catch (const std::exception& e) {
            NOVA_LOG(("Failed to load configuration: " + std::string(e.what())).c_str(), LogType::Error);
        }
    }

    void OptionsMenu::SaveConfig()
    {
        NOVA_LOG("Saving configuration...", LogType::Log);
        try {
            std::string configPath = std::filesystem::current_path().string() + "/Config/app_config.json";
            nlohmann::json configJson;

            configJson["clearContent"] = options_.clearContent;
            configJson["noRoot"] = options_.noRoot;
            configJson["verbose"] = options_.verbose;
            configJson["requestRootForBrew"] = options_.requestRootForBrew;
            configJson["requestRootForPip"] = options_.requestRootForPip;
            configJson["requestRootForVenv"] = options_.requestRootForVenv;
            configJson["scrollableLogAlwaysVisible"] = options_.scrollableLogAlwaysVisible;
            configJson["disableMousePartyMode"] = options_.disableMousePartyMode;
            configJson["mkdocsProjectPath"] = options_.mkdocsProjectPath;

            std::ofstream configFile(configPath);
            configFile << configJson.dump(4);

            NOVA_LOG("Configuration saved successfully.", LogType::Log);
        } catch (const std::exception& e) {
            NOVA_LOG(("Failed to save configuration: " + std::string(e.what())).c_str(), LogType::Error);
        }
    }

    void OptionsMenu::Show()
    {
        using namespace ftxui;
        NOVA_LOG("Showing enhanced options menu!", LogType::Log);

        auto start_time = std::chrono::steady_clock::now();
        bool show_particles = true;

        // Create styled checkbox with improved visual design and clickable labels
        auto makeStyledCheckbox = [&](const std::string& label, bool* value) {
            // Use direct checkbox component to maintain interactivity
            return Checkbox(label, value) | color(Color::Cyan);
        };

        // Styled input field with more compact centered content
        auto makeStyledInput = [](const std::string& label, std::string* value) {
            // Create a vertical container with the label and input to maintain interactivity
            return Container::Vertical({
                // Fix: Use direct text element for the label
                Renderer([=] {
                    return text(label) | ftxui::bold | color(Color::Yellow);
                }),
                Input(value, "") | border | size(WIDTH, LESS_THAN, 30)
            });
        };

        // Create checkboxes for boolean options with better styling
        auto clearContentCheckbox = makeStyledCheckbox("Clear Content", &options_.clearContent);
        auto noRootCheckbox = makeStyledCheckbox("No Root Access", &options_.noRoot);
        auto verboseCheckbox = makeStyledCheckbox("Verbose Mode", &options_.verbose);
        auto requestRootForBrewCheckbox = makeStyledCheckbox("Request Root for Homebrew", &options_.requestRootForBrew);
        auto requestRootForPipCheckbox = makeStyledCheckbox("Request Root for Pip", &options_.requestRootForPip);
        auto requestRootForVenvCheckbox = makeStyledCheckbox("Request Root for Virtual Environment", &options_.requestRootForVenv);
        auto scrollableLogAlwaysVisibleCheckbox = makeStyledCheckbox("Scrollable Log Always Visible", &options_.scrollableLogAlwaysVisible);
        auto disableMousePartyModeCheckbox = makeStyledCheckbox("Disable Mouse Party Mode (Light Party)", &options_.disableMousePartyMode);

        // Create text box for string options with better styling
        auto mkdocsProjectPathTextBox = makeStyledInput("MkDocs Project Path", &options_.mkdocsProjectPath);

        // Styled back button - centered properly
        auto BackButton = Button("🔙 Save & Return to Main Menu", [&]() { 
            SaveConfig(); // Save configuration before going back
            GetScreen().ExitLoopClosure()(); 
        });
        
        // Menu container with minimal spacing and two-column layout for options
        auto menu = Container::Vertical({
            // Create a two-column layout to save vertical space
            Container::Horizontal({
                Container::Vertical({
                    clearContentCheckbox,
                    noRootCheckbox,
                    verboseCheckbox,
                    requestRootForBrewCheckbox,
                }),
                Container::Vertical({
                    requestRootForPipCheckbox,
                    requestRootForVenvCheckbox,
                    scrollableLogAlwaysVisibleCheckbox,
                    disableMousePartyModeCheckbox,
                }),
            }),
            
            // MkDocs path and back button
            mkdocsProjectPathTextBox,
            Container::Horizontal({
                Renderer([] { return text(" ") | flex; }),
                BackButton,
                Renderer([] { return text(" ") | flex; }),
            }) | center,
        });

        // Main component with animations - ultra compact for 65x35 space
        auto component = Renderer(menu, [=, &start_time, &show_particles]() -> Element {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();

            // Create animated rainbow colors
            int color_cycle = (elapsed / 100) % 360;
            auto get_rainbow_color = [&](int offset) -> Color {
                int hue = (color_cycle + offset) % 360;
                if (hue < 60) return Color::Red;
                else if (hue < 120) return Color::Yellow; 
                else if (hue < 180) return Color::Green;
                else if (hue < 240) return Color::Cyan;
                else if (hue < 300) return Color::Blue;
                else return Color::Magenta;
            };

            // Ultra compact title - just one line
            auto title = text("OPTIONS") | ftxui::bold | color(get_rainbow_color(0)) | center;

            // Minimal particle effect
            auto createParticles = [&]() -> Element {
                if (!show_particles) return text("");
                Elements particles;
                for (int x = 0; x < 10; ++x) {
                    auto particle_time = (elapsed + x * 111) / 50;
                    auto char_idx = particle_time % 3;
                    // Use string constructor to properly concatenate strings
                    std::string particle_char = char_idx == 0 ? "· " : (char_idx == 1 ? "○ " : "✦ ");
                    particles.push_back(text(particle_char) | color(get_rainbow_color(x * 30)));
                }
                return hbox(particles) | center;
            };

            // Complete layout with minimal elements - optimized for 65x35
            return vbox({
                // Particle effect
                createParticles(),
                
                // Compact title
                hbox({
                    text("[ "),
                    title,
                    text(" ]"),
                }) | color(get_rainbow_color(90)) | center,
                
                // Menu content with minimal decoration
                menu->Render() | border | color(get_rainbow_color(180)),
                
                // Footer with essential controls only
                text("ESC:Back | P:Effects") | center | color(Color::Yellow),
            })
            | size(WIDTH, EQUAL, 65)  // Width: 65
            | size(HEIGHT, EQUAL, 35) // Height: reduced to 35
            | border | center;
        });

        // Add keyboard handling for particle toggle and escape
        auto enhanced_component = CatchEvent(component, [&](Event event) {
            if (event == Event::Character('p') || event == Event::Character('P')) {
                show_particles = !show_particles;
                return true;
            }
            if (event == Event::Escape) {
                SaveConfig(); // Save configuration when exiting with ESC
                GetScreen().ExitLoopClosure()();
                return true;
            }
            return false;
        });

        GetScreen().Loop(enhanced_component);
        SaveConfig(); // Save configuration when closing the menu
    }

    void OptionsMenu::Hide()
    {
        NOVA_LOG("Hiding options menu!", LogType::Log);
        GetScreen().ExitLoopClosure()();
    }

    void OptionsMenu::RegisterCommandLineOptions()
    {
        NOVA_LOG("Registering command-line options dynamically...", LogType::Log);

        commandLineMapping_ = {
            {"clearContent", options_.clearContent ? new std::string("true") : new std::string("false")},
            {"noRoot", options_.noRoot ? new std::string("true") : new std::string("false")},
            {"verbose", options_.verbose ? new std::string("true") : new std::string("false")},
            {"requestRootForBrew", options_.requestRootForBrew ? new std::string("true") : new std::string("false")},
            {"requestRootForPip", options_.requestRootForPip ? new std::string("true") : new std::string("false")},
            {"requestRootForVenv", options_.requestRootForVenv ? new std::string("true") : new std::string("false")},
            {"scrollableLogAlwaysVisible", options_.scrollableLogAlwaysVisible ? new std::string("true") : new std::string("false")},
            {"mkdocsProjectPath", &options_.mkdocsProjectPath} // Keep pointer
        };

       // Register each option with its associated behavior
        optionManager_->RegisterOption("clearContent", options_.clearContent ? new std::string("true") : new std::string("false"), []() {
            std::string contentFolderPath = "Content";
            if (Core::FileOperations::NovaFileOperations::DirectoryExists(contentFolderPath)) {
                Core::FileOperations::NovaFileOperations::DeleteDirectory(contentFolderPath);
                NOVA_LOG("Content directory cleared", LogType::Log);
                Core::FileOperations::NovaFileOperations::CreateDirectory(contentFolderPath);
                Core::FileOperations::NovaFileOperations::CreateDirectory(contentFolderPath + "/Logs");
                Core::FileOperations::NovaFileOperations::CreateDirectory(contentFolderPath + "/Website");
                NOVA_LOG("Content directories recreated", LogType::Log);
            }
        });

        optionManager_->RegisterOption("noRoot", options_.noRoot ? new std::string("true") : new std::string("false"), []() {
            NOVA_LOG("No root access mode enabled", LogType::Log);
        });

        optionManager_->RegisterOption("verbose", options_.verbose ? new std::string("true") : new std::string("false"), []() {
            NOVA_LOG("Verbose mode enabled", LogType::Log);
        });

        optionManager_->RegisterOption("requestRootForBrew", options_.requestRootForBrew ? new std::string("true") : new std::string("false"), []() {
            NOVA_LOG("Requesting root access for Homebrew", LogType::Log);
        });

        optionManager_->RegisterOption("requestRootForPip", options_.requestRootForPip ? new std::string("true") : new std::string("false"), []() {
            NOVA_LOG("Requesting root access for Pip", LogType::Log);
        });

        optionManager_->RegisterOption("requestRootForVenv", options_.requestRootForVenv ? new std::string("true") : new std::string("false"), []() {
            NOVA_LOG("Requesting root access for Virtual Environment", LogType::Log);
        });

        optionManager_->RegisterOption("scrollableLogAlwaysVisible", options_.scrollableLogAlwaysVisible ? new std::string("true") : new std::string("false"), []() {
            auto scrollableLog = Utils::ScrollableLog::Create();
            scrollableLog->SetAlwaysVisible(true);
            NOVA_LOG("Scrollable log visibility set to always visible", LogType::Log);
        });

        optionManager_->RegisterOption("disableMousePartyMode", options_.disableMousePartyMode ? new std::string("true") : new std::string("false"), []() {
            NOVA_LOG("Mouse Party Mode (Light Party) disabled", LogType::Log);
        });

        optionManager_->RegisterOption("mkdocsProjectPath", &options_.mkdocsProjectPath, [this]() {
            NOVA_LOG(("MkDocs project path set to: " + options_.mkdocsProjectPath).c_str(), LogType::Log);
        });

        NOVA_LOG("Command-line options registered successfully.", LogType::Log);
    }

    void OptionsMenu::ApplyCommandLineOptions(const CommandLineOptionsStruct& cmdOptions)
    {
        NOVA_LOG("Applying command-line options dynamically...", LogType::Log);

        for (const auto& [key, value] : commandLineMapping_)
        {
            if (optionManager_->IsOptionEnabled(key)) // Check if the command-line argument is set
            {
                *value = true; // Override the corresponding menu option
                NOVA_LOG(("Command-line option '" + key + "' applied: " + (optionManager_->IsOptionEnabled(key) ? "Enabled" : "Disabled")).c_str(), LogType::Log);
            }
        }

        NOVA_LOG("Command-line options applied successfully.", LogType::Log);
    }
}
