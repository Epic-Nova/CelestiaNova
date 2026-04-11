#include "MainMenu.h"
#include "MenuMinimal.h"
#include "NovaCore.h"
#include <chrono>
#include <cmath>

namespace Menus
{
    using namespace ftxui;
    using namespace Core;

    std::shared_ptr<MainMenu> MainMenu::InitializeMainMenu()
    {
        auto MainMenuSharedPtr = std::make_shared<MainMenu>();
        return MainMenuSharedPtr;
    }

    void MainMenu::Show()
    {
        NOVA_LOG("Showing main menu!", LogType::Log);

        // Animation state variables
        auto start_time = std::chrono::steady_clock::now();
        bool show_particles = true;

        // Create custom button style function with bold text
        auto makeCustomButton = [](const std::string& label, std::function<void()> callback, Color bg, Color fg, Color bg_active, Color fg_active) {
            auto option = ButtonOption::Animated(bg, fg, bg_active, fg_active);
            option.transform = [](const EntryState& state) {
                std::string prefix = state.focused ? "▶ " : "  ";
                std::string suffix = state.focused ? " ◀" : "  ";
                
                // Create bold button text with additional padding
                return hbox({
                    text(prefix),
                    text(state.label) | ftxui::bold | size(WIDTH, GREATER_THAN, 30),
                    text(suffix),
                }) | center | size(HEIGHT, GREATER_THAN, 1);
            };
            return Button(label, callback, option);
        };

        // Create fancy animated buttons with special effects and bold text
        auto InstallRequirementsButton = makeCustomButton(
            "🚀 Install Requirements", 
            [&]() {
                NOVA_LOG("Opening installation menu for confirmation...", LogType::Log);
                auto installMenu = Menus::InstallationMenu::Create();
                installMenu->Show(); // Show the installation menu without starting the installation
            },
            Color::Black, Color::Magenta, Color::Magenta, Color::White
        );
        
        auto StartDocumentationWebpageButton = makeCustomButton(
            "📚 Start Documentation", 
            *GetMenuActionCallback("StartDocumentationWebpage"),
            Color::Black, Color::Cyan, Color::Cyan, Color::White
        );
        
        auto OptionsButton = makeCustomButton(
            "⚙️  Options", 
            *GetMenuActionCallback("Options"),
            Color::Black, Color::Yellow, Color::Yellow, Color::Black
        );
        
        auto ExtensionsButton = makeCustomButton(
            "🔌 Extensions",
            *GetMenuActionCallback("Extensions"),
            Color::Black, Color::Blue, Color::Blue, Color::White
        );
        
        auto HelpButton = makeCustomButton(
            "❓ Help", 
            *GetMenuActionCallback("ShowHelp"),
            Color::Black, Color::Green, Color::Green, Color::White
        );
        
        auto QuitButton = makeCustomButton(
            "🚪 Quit", 
            *GetMenuActionCallback("Quit"),
            Color::Black, Color::Red, Color::Red, Color::White
        );
        
        NOVA_LOG("Created main menu buttons!", LogType::Log);

        // Create centered menu with buttons and more vertical spacing
        auto menu = Container::Vertical({
            InstallRequirementsButton,
            Renderer([] { return text(""); }), // Add spacing between buttons
            StartDocumentationWebpageButton,
            Renderer([] { return text(""); }),
            OptionsButton,
            Renderer([] { return text(""); }),
            ExtensionsButton,
            Renderer([] { return text(""); }),
            HelpButton,
            Renderer([] { return text(""); }),
            QuitButton
        });

        auto component = Renderer(menu, [=, &start_time, &show_particles]() mutable -> Element {
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

            // Animated ASCII art title
            auto wave_offset = (elapsed / 200) % 20;
            auto create_wave_title = [&]() -> Element {
                Elements title_lines;
                std::vector<std::string> ascii_art = {
                    "███╗   ██╗ ██████╗ ██╗   ██╗ █████╗ ",
                    "████╗  ██║██╔═══██╗██║   ██║██╔══██╗",
                    "██╔██╗ ██║██║   ██║██║   ██║███████║",
                    "██║╚██╗██║██║   ██║╚██╗ ██╔╝██╔══██║",
                    "██║ ╚████║╚██████╔╝ ╚████╔╝ ██║  ██║",
                    "╚═╝  ╚═══╝ ╚═════╝   ╚═══╝  ╚═╝  ╚═╝"
                };
                
                for (size_t i = 0; i < ascii_art.size(); ++i) {
                    auto line_color = get_rainbow_color(i * 60 + wave_offset * 18);
                    title_lines.push_back(text(ascii_art[i]) | color(line_color) | center);
                }
                return vbox(title_lines);
            };

            // Pulsing subtitle
            auto pulse = std::sin(elapsed * 0.005) * 0.5 + 0.5;
            auto subtitle_color = pulse > 0.7 ? Color::White : 
                                 pulse > 0.4 ? Color::GrayLight : Color::GrayDark;

            // Create animated particle field background
            auto create_particles = [&]() -> Element {
                if (!show_particles) return text("");
                
                Elements particle_lines;
                for (int i = 0; i < 3; ++i) {
                    Elements particles;
                    for (int j = 0; j < 40; ++j) {
                        auto particle_time = (elapsed + i * 333 + j * 111) / 50;
                        auto particle_chars = std::vector<std::string>{"·", "⋅", "∘", "○", "●", "★", "✦", "✧"};
                        auto char_idx = (particle_time / 4) % particle_chars.size();
                        auto spacing = (j + particle_time) % 3 == 0 ? "   " : "    ";
                        particles.push_back(text(particle_chars[char_idx] + spacing) | 
                                          color(get_rainbow_color(j * 9 + i * 120)));
                    }
                    particle_lines.push_back(hbox(particles));
                }
                return vbox(particle_lines) | dim;
            };

            // Animated side decorations
            auto create_side_decoration = [&](bool left) -> Element {
                Elements deco;
                auto symbols = std::vector<std::string>{"▲", "◆", "●", "■", "▼", "◇", "○", "□"};
                for (int i = 0; i < 8; ++i) {
                    auto symbol_idx = (elapsed / 200 + i * (left ? 1 : -1)) % symbols.size();
                    auto color_offset = i * 45 + (left ? 0 : 180);
                    deco.push_back(text(symbols[symbol_idx]) | color(get_rainbow_color(color_offset)));
                }
                return vbox(deco);
            };

            // Glowing effect for menu buttons - Fixed to handle events
            auto glowing_menu = [=]() -> Element {
                auto menu_element = menu->Render();
                auto glow_intensity = std::sin(elapsed * 0.003) * 0.3 + 0.7;
                auto glow_color = glow_intensity > 0.8 ? Color::White : Color::GrayLight;
                
                return vbox({
                    text("") | color(glow_color),
                    menu_element | border | center | ftxui::bold | color(glow_color),
                    text("") | color(glow_color)
                });
            };

            // Animated status bar
            auto create_status_bar = [&]() -> Element {
                auto progress_chars = std::vector<std::string>{"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
                Elements progress_elements;
                for (int i = 0; i < 30; ++i) {
                    auto height_idx = (elapsed / 100 + i * 3) % progress_chars.size();
                    auto bar_color = get_rainbow_color(i * 12);
                    progress_elements.push_back(text(progress_chars[height_idx]) | color(bar_color));
                }
                return hbox(progress_elements) | center;
            };

            // Main layout with all animations
            return vbox({
                // Particle background
                create_particles(),
                text(""),

                // Animated title section
                vbox({
                    create_wave_title(),
                    text(""),
                    text("CELESTIA NOVA") | color(subtitle_color) | center | ftxui::bold,
                    text(""),
                }) | border | color(get_rainbow_color(0)),


                // Main content with side decorations
                hbox({
                    create_side_decoration(true) | size(WIDTH, EQUAL, 3),
                    text(" "),
                    vbox({
                        text("┌" + std::string(50, '-') + "┐") | color(get_rainbow_color(90)),
                        hbox({
                            text("│"),
                            text("      Welcome to Celestia Nova!    ") | center,
                            text("│")
                        }) | color(Color::White),
                        hbox({
                            text("│"),
                            text("          Please select an option below:          ") | center,
                            text("│")
                        }) | color(Color::GrayLight),
                        text("└" + std::string(50, '-') + "┘") | color(get_rainbow_color(90)),
                        text(""),
                        
                        // Glowing menu - Fixed to render inline
                        glowing_menu(),
                        
                        text(""),
                        create_status_bar(),
                    }) | flex,
                    text(" "),
                    create_side_decoration(false) | size(WIDTH, EQUAL, 3),
                }) | center,

                text(""),

                // Footer with animated elements
                vbox({
                    text(std::string(47, '=')) | 
                        color(get_rainbow_color(180)) | center,
                    hbox({
                        text("✨ Press ESC to quit ✨") | color(Color::Yellow),
                        text(" | ") | color(Color::GrayDark),
                        text("🎮 Use arrow keys to navigate 🎮") | color(Color::Cyan),
                    }) | center,
                    text(std::string(47, '=')) | 
                        color(get_rainbow_color(180)) | center,
                }) | border | color(get_rainbow_color(270)),

            }) 
            | size(WIDTH,  EQUAL, 81)   // fixed width
            | size(HEIGHT, EQUAL, 60)   // fixed height - increased from 48 to 60
            | center;  // centered but not resizable container
        });

        // Add keyboard handling for particle toggle and let menu handle button events
        auto enhanced_component = CatchEvent(component, [&](Event event) {
            if (event == Event::Character('p') || event == Event::Character('P')) {
                show_particles = !show_particles;
                return true;
            }
            return false;  // Let the underlying menu component handle button events
        });

        // Use default screen instead of creating a new one
        GetScreen().Loop(enhanced_component);
    }

    void MainMenu::Hide()
    {
        NOVA_LOG("Hiding main menu!", LogType::Log);
        GetScreen().ExitLoopClosure()();
    }

    Component MainMenu::Wrap(std::string name, Component component)
    {
        return Renderer(component, [component, name] {
            return window(text(name), component->Render());
        });
    }
}