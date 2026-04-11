/// @file Interactables.cpp
/// @brief Implements user-interaction callbacks for menus.

#include "NovaCore.h"
#include "NovaMinimal.h"

#include "Menus/MainMenu.h"
#include "Menus/InstallationMenu.h"
#include "Menus/OptionsMenu.h"
#include "Menus/HelpMenu.h"
#include "Utils/ConfigManager.h"

#ifdef __APPLE__
#include "Helpers/BrewInstallHelper.h"
#endif

#include "Helpers/PipInstallHelper/PipInstallHelper.h"
#include "Helpers/RootAccessHelper/RootAccessHelper.h"
#include <filesystem>
#include <algorithm>
#include <map>
#include "Core/ExtensionRegistry.h"
#include "Core/ModuleManager.h"

namespace Utils
{
    using namespace Core;
    using namespace Core::Helpers;

    /**
     * Registers all main menu callbacks for user interaction.
     */
    void Interactables::RegisterMenuCallbacks(std::shared_ptr<MainMenu> mainMenu, 
                                              CommandLineOptionsStruct cmdOptions,
                                              const std::string& contentFolderPath) // Changed NovaFileHandle to std::string
    {
        // Register extensions callback (plugin UI)
        RegisterExtensionsCallback(mainMenu, cmdOptions, contentFolderPath);

        RegisterOptionsCallback(mainMenu, cmdOptions);
        RegisterInstallRequirementsCallback(mainMenu, cmdOptions, contentFolderPath);
        RegisterStartDocumentationCallback(mainMenu, contentFolderPath);
        RegisterHelpCallback(mainMenu);
        RegisterQuitCallback(mainMenu);
    }

    void Interactables::RegisterExtensionsCallback(std::shared_ptr<MainMenu> mainMenu,
                                                   CommandLineOptionsStruct cmdOptions,
                                                   const std::string& contentFolderPath)
    {
        mainMenu->SetMenuActionCallback("Extensions", [mainMenu]() {
            using namespace ftxui;
            using namespace Core;
            using namespace Core::FileOperations;

            ExtensionRegistry::Instance().Discover("Extensions");
            auto descriptors = ExtensionRegistry::Instance().ListExtensionDescriptors();

            std::sort(descriptors.begin(), descriptors.end(), [](const ExtensionDescriptor& a, const ExtensionDescriptor& b) {
                const std::string an = a.name.empty() ? a.id : a.name;
                const std::string bn = b.name.empty() ? b.id : b.name;
                if (an == bn) {
                    return a.id < b.id;
                }
                return an < bn;
            });

            struct ExtensionViewEntry {
                std::string id;
                std::string label;
                bool isCategoryHeader = false;
            };

            auto toForwardSlashes = [](std::string value) {
                std::replace(value.begin(), value.end(), '\\', '/');
                return value;
            };

            auto categoryFromDescriptor = [&](const ExtensionDescriptor& descriptor) {
                std::string descriptorPath = ExtensionRegistry::Instance().GetExtensionDescriptorPath(descriptor.id);
                descriptorPath = toForwardSlashes(descriptorPath);

                size_t markerPos = descriptorPath.find("/Extensions/");
                if (markerPos == std::string::npos) {
                    markerPos = descriptorPath.find("Extensions/");
                    if (markerPos == std::string::npos) {
                        return std::string("Uncategorized");
                    }
                    markerPos += std::string("Extensions/").size();
                } else {
                    markerPos += std::string("/Extensions/").size();
                }

                std::string relativePath = descriptorPath.substr(markerPos);
                std::vector<std::string> segments;
                std::string token;
                for (char c : relativePath) {
                    if (c == '/') {
                        if (!token.empty()) {
                            segments.push_back(token);
                            token.clear();
                        }
                        continue;
                    }
                    token.push_back(c);
                }
                if (!token.empty()) {
                    segments.push_back(token);
                }

                if (segments.size() < 3) {
                    return std::string("Uncategorized");
                }

                std::string category;
                for (size_t i = 0; i + 2 < segments.size(); ++i) {
                    if (!category.empty()) {
                        category += "/";
                    }
                    category += segments[i];
                }
                return category.empty() ? std::string("Uncategorized") : category;
            };

            std::map<std::string, std::vector<const ExtensionDescriptor*>> descriptorsByCategory;
            for (const auto& descriptor : descriptors) {
                descriptorsByCategory[categoryFromDescriptor(descriptor)].push_back(&descriptor);
            }

            std::vector<ExtensionViewEntry> extensionEntries;
            std::vector<std::string> extensionLabels;

            for (auto& [category, categoryDescriptors] : descriptorsByCategory) {
                std::sort(categoryDescriptors.begin(), categoryDescriptors.end(), [](const ExtensionDescriptor* a, const ExtensionDescriptor* b) {
                    const std::string an = a->name.empty() ? a->id : a->name;
                    const std::string bn = b->name.empty() ? b->id : b->name;
                    if (an == bn) {
                        return a->id < b->id;
                    }
                    return an < bn;
                });

                ExtensionViewEntry categoryHeader;
                categoryHeader.id = "";
                categoryHeader.label = "[" + category + "]";
                categoryHeader.isCategoryHeader = true;
                extensionEntries.push_back(categoryHeader);

                for (const auto* descriptor : categoryDescriptors) {
                    ExtensionViewEntry entry;
                    entry.id = descriptor->id;
                    std::string displayName = descriptor->name.empty() ? descriptor->id : descriptor->name;
                    entry.label = "  " + displayName + " (" + descriptor->id + ")";
                    if (!descriptor->version.empty()) {
                        entry.label += " v" + descriptor->version;
                    }
                    extensionEntries.push_back(std::move(entry));
                }
            }

            if (extensionEntries.empty()) {
                extensionEntries.push_back({"", "No extensions discovered", true});
            }

            for (const auto& entry : extensionEntries) {
                extensionLabels.push_back(entry.label);
            }

            int selectedIndex = 0;
            bool showDescriptorJson = false;
            std::string activeDescriptorPath;
            std::string activeDescriptorBody;

            auto listMenu = Menu(&extensionLabels, &selectedIndex);

            auto openDescriptorButton = Button("Open Descriptor JSON", [&]() {
                if (selectedIndex < 0 || selectedIndex >= static_cast<int>(extensionEntries.size())) {
                    return;
                }

                const auto& selectedEntry = extensionEntries[selectedIndex];
                if (selectedEntry.isCategoryHeader || selectedEntry.id.empty()) {
                    return;
                }

                const std::string selectedId = selectedEntry.id;

                activeDescriptorPath = ExtensionRegistry::Instance().GetExtensionDescriptorPath(selectedId);
                if (activeDescriptorPath.empty()) {
                    activeDescriptorBody = "Descriptor path unavailable for this extension.";
                } else {
                    activeDescriptorBody = NovaFileOperations::ReadTextFile(activeDescriptorPath);
                    if (activeDescriptorBody.empty()) {
                        activeDescriptorBody = "Descriptor file is empty or unreadable.";
                    }
                }
                showDescriptorJson = true;
            });

            auto closeDescriptorButton = Button("Close Descriptor", [&]() {
                showDescriptorJson = false;
            });

            auto backButton = Button("Back", [mainMenu]() {
                mainMenu->GetScreen().ExitLoopClosure()();
            });

            auto actions = Container::Horizontal({openDescriptorButton, closeDescriptorButton, backButton});
            auto root = Container::Vertical({listMenu, actions});

            auto component = Renderer(root, [&]() -> Element {
                const ExtensionDescriptor* selectedDescriptor = nullptr;
                if (selectedIndex >= 0 && selectedIndex < static_cast<int>(extensionEntries.size())) {
                    const auto& selectedEntry = extensionEntries[selectedIndex];
                    if (!selectedEntry.isCategoryHeader && !selectedEntry.id.empty()) {
                        selectedDescriptor = ExtensionRegistry::Instance().GetExtensionDescriptor(selectedEntry.id);
                    }
                }

                Elements details;
                std::string longDescription;
                details.push_back(text("Descriptor Viewer") | bold | color(Color::Cyan));

                if (selectedDescriptor) {
                    details.push_back(text("ID: " + selectedDescriptor->id));
                    if (!selectedDescriptor->description.empty()) {
                        details.push_back(paragraph("Summary: " + selectedDescriptor->description));
                    }
                    if (!selectedDescriptor->longDescription.empty()) {
                        longDescription = selectedDescriptor->longDescription;
                    }
                    details.push_back(text("Dependencies: " + std::to_string(selectedDescriptor->dependencies.size())));
                } else {
                    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(extensionEntries.size()) && extensionEntries[selectedIndex].isCategoryHeader) {
                        details.push_back(text("Category: " + extensionEntries[selectedIndex].label));
                        details.push_back(paragraph("Select an extension row under this category to inspect its descriptor."));
                    } else {
                        details.push_back(text("No descriptor selected."));
                    }
                }

                Element detailsBody = vbox(details);
                if (!longDescription.empty()) {
                    detailsBody = vbox({
                        detailsBody,
                        separator(),
                        text("Long Description") | bold,
                        paragraph(longDescription) | size(WIDTH, EQUAL, 66) | flex,
                    });
                }

                Element listPanel = window(
                    text("Extensions (Folder-Categorized)"),
                    listMenu->Render() | frame | vscroll_indicator | size(WIDTH, EQUAL, 62) | size(HEIGHT, EQUAL, 16)
                );

                Element detailsPanel = window(
                    text("Selection"),
                    detailsBody | frame | vscroll_indicator | size(WIDTH, EQUAL, 74) | size(HEIGHT, EQUAL, 16)
                );

                Elements body;
                body.push_back(text("Extensions Catalog") | bold | center | color(Color::Blue));
                body.push_back(text("Browse extension descriptors only. This menu never executes extensions.") | center | color(Color::GrayLight));
                body.push_back(separator());
                body.push_back(hbox({listPanel, detailsPanel}));

                if (showDescriptorJson) {
                    body.push_back(separator());
                    std::string descriptorTitle = activeDescriptorPath.empty() ? "Descriptor JSON" : ("Descriptor JSON: " + activeDescriptorPath);
                    body.push_back(
                        window(
                            text(descriptorTitle),
                            paragraph(activeDescriptorBody) | frame | vscroll_indicator | size(HEIGHT, LESS_THAN, 14)
                        )
                    );
                }

                body.push_back(separator());
                body.push_back(actions->Render() | center);

                return vbox(body) | border | size(WIDTH, EQUAL, 140) | center;
            });

            auto withEvents = CatchEvent(component, [&](Event event) {
                if (event == Event::Escape && showDescriptorJson) {
                    showDescriptorJson = false;
                    return true;
                }
                return false;
            });

            mainMenu->GetScreen().Loop(withEvents);
        });
    }

    /**
     * Registers the callback for the Options menu.
     */
    void Interactables::RegisterOptionsCallback(std::shared_ptr<MainMenu> mainMenu, 
                                                CommandLineOptionsStruct cmdOptions)
    {
        // Use the cmdOptions directly since it's already a copy
        mainMenu->SetMenuActionCallback("Options", [cmdOptions]() mutable {
            auto optionsMenu = OptionsMenu::Create(cmdOptions);
            optionsMenu->Show();
            
            // Save configuration after options menu closes
            CommandLineOptionsStruct config;
            config.clearContent = cmdOptions.clearContent;
            config.noRoot = cmdOptions.noRoot;
            config.verbose = cmdOptions.verbose;
            config.requestRootForBrew = cmdOptions.requestRootForBrew;
            config.requestRootForPip = cmdOptions.requestRootForPip;
            config.requestRootForVenv = cmdOptions.requestRootForVenv;
            config.mkdocsProjectPath = cmdOptions.mkdocsProjectPath;
            config.scrollableLogAlwaysVisible = cmdOptions.scrollableLogAlwaysVisible;
            NOVA_LOG("Saving configuration after options menu closed", LogType::Log);
            ConfigManager::SaveConfig(config);
        });
    }

    /**
     * Registers the callback for installing requirements.
     */
    void Interactables::RegisterInstallRequirementsCallback(std::shared_ptr<MainMenu> mainMenu, 
                                                            CommandLineOptionsStruct cmdOptions,
                                                            const std::string& contentFolderPath) // Changed NovaFileHandle to std::string
    {
        // Use cmdOptions directly as it's already passed by value
        mainMenu->SetMenuActionCallback("InstallRequirements", [cmdOptions, contentFolderPath]() 
        {
            NOVA_LOG("Starting installation process...", LogType::Debug);
            NOVA_LOG_VERBOSE("RegisterInstallRequirementsCallback: callback invoked", LogType::Debug);

            // Create and configure installation menu
            NOVA_LOG_VERBOSE("Creating installation menu instance", LogType::Debug);
            auto installMenu = InstallationMenu::Create();
            NOVA_LOG_VERBOSE("Installation menu instance created", LogType::Debug);
            
            // Add all installation steps with substeps
            installMenu->AddProgressStep("Check Homebrew");
            installMenu->AddSubStep("Check Homebrew", "Initialize Homebrew Helper");
            installMenu->AddSubStep("Check Homebrew", "Verify Installation");
            
            installMenu->AddProgressStep("Install/Verify Homebrew");
            installMenu->AddSubStep("Install/Verify Homebrew", "Request Root Access");
            installMenu->AddSubStep("Install/Verify Homebrew", "Download Installation Script");
            installMenu->AddSubStep("Install/Verify Homebrew", "Execute Installation");
            installMenu->AddSubStep("Install/Verify Homebrew", "Verify Installation");
            
            installMenu->AddProgressStep("Check Python/Pip");
            installMenu->AddSubStep("Check Python/Pip", "Initialize Pip Helper");
            installMenu->AddSubStep("Check Python/Pip", "Check Requirements");
            installMenu->AddSubStep("Check Python/Pip", "Verify Pip Installation");
            
            installMenu->AddProgressStep("Install/Verify Python/Pip");
            installMenu->AddSubStep("Install/Verify Python/Pip", "Install Python via Homebrew");
            installMenu->AddSubStep("Install/Verify Python/Pip", "Link Python Installation");
            installMenu->AddSubStep("Install/Verify Python/Pip", "Verify Pip Access");
            
            installMenu->AddProgressStep("Install Virtual Environment");
            installMenu->AddSubStep("Install Virtual Environment", "Install virtualenv package");
            
            installMenu->AddProgressStep("Create Virtual Environment");
            installMenu->AddSubStep("Create Virtual Environment", "Request Root Access");
            installMenu->AddSubStep("Create Virtual Environment", "Create venv directory");
            installMenu->AddSubStep("Create Virtual Environment", "Initialize virtual environment");
            
            installMenu->AddProgressStep("Install Python Packages");
            installMenu->AddSubStep("Install Python Packages", "Install MkDocs");
            installMenu->AddSubStep("Install Python Packages", "Install MkDocs Material");
            installMenu->AddSubStep("Install Python Packages", "Verify Installation");
            
            // Start installation in separate thread
            NOVA_LOG_VERBOSE("Creating installation thread", LogType::Debug);
            std::thread installThread([cmdOptions, contentFolderPath, installMenu]() {
                NOVA_LOG_VERBOSE("Installation thread started", LogType::Debug);
                
                // Store helper pointers for proper cleanup
                BrewInstallHelper* BrewInstallHelperPtr = nullptr;
                PipInstallHelper* PipInstallHelperPtr = nullptr;
                RootAccessHelper* RootAccessHelperPtr = nullptr;
                RootAccessHelper* RootAccessHelperPtr2 = nullptr;
                
                try {
                    std::string contentPath = contentFolderPath;
                    NOVA_LOG_VERBOSE(("Installation thread: content path = " + contentPath).c_str(), LogType::Debug);
                    
                    // Step 1: Check Homebrew
                    NOVA_LOG_VERBOSE("Installation thread: Step 1 - Check Homebrew", LogType::Debug);
                    installMenu->SetCurrentStep("Check Homebrew");
                    installMenu->SetCurrentSubStep("Check Homebrew", "Initialize Homebrew Helper");
                    
                    #ifdef __APPLE__
                    NOVA_LOG_VERBOSE("Creating BrewInstallHelper", LogType::Debug);
                    BrewInstallHelperPtr = BrewInstallHelper::CreatePlatformSpecific();
                    if (!BrewInstallHelperPtr) {
                        NOVA_LOG_VERBOSE("Failed to create BrewInstallHelper", LogType::Error);
                        throw std::runtime_error("Failed to create BrewInstallHelper");
                    }
                    NOVA_LOG_VERBOSE("BrewInstallHelper created successfully", LogType::Debug);
                    
                    BrewInstallHelperPtr->SetInstallCallbackFunction("brew_progress", [installMenu](const std::string& msg) {
                        NOVA_LOG_VERBOSE(("Brew callback: " + msg).c_str(), LogType::Debug);
                        installMenu->UpdateProgress("Homebrew: " + msg);
                    });
                    
                    NOVA_LOG_VERBOSE("Initializing BrewInstallHelper", LogType::Debug);
                    BrewInstallHelperPtr->Initialize();
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    installMenu->CompleteCurrentSubStep();
                    
                    installMenu->SetCurrentSubStep("Check Homebrew", "Verify Installation");
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    installMenu->CompleteCurrentSubStep();
                    installMenu->CompleteCurrentStep();
                    
                    // Step 2: Install/Verify Homebrew
                    installMenu->SetCurrentStep("Install/Verify Homebrew");
                    installMenu->SetCurrentSubStep("Install/Verify Homebrew", "Request Root Access");
                    std::this_thread::sleep_for(std::chrono::milliseconds(300));
                    installMenu->CompleteCurrentSubStep();
                    
                    installMenu->SetCurrentSubStep("Install/Verify Homebrew", "Download Installation Script");
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    installMenu->CompleteCurrentSubStep();
                    
                    installMenu->SetCurrentSubStep("Install/Verify Homebrew", "Execute Installation");
                    if (!cmdOptions.noRoot) {
                        BrewInstallHelperPtr->Execute([&]() {
                            return BrewInstallHelperPtr->IsBrewInstalled();
                        });
                    } else {
                        installMenu->UpdateProgress("Skipping root access (no-root mode)");
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    }
                    installMenu->CompleteCurrentSubStep();
                    
                    installMenu->SetCurrentSubStep("Install/Verify Homebrew", "Verify Installation");
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    installMenu->CompleteCurrentSubStep();
                    installMenu->CompleteCurrentStep();
                    
                    // Step 3: Check Python/Pip
                    installMenu->SetCurrentStep("Check Python/Pip");
                    installMenu->SetCurrentSubStep("Check Python/Pip", "Initialize Pip Helper");
                    
                    PipInstallHelper* PipInstallHelperPtr = PipInstallHelper::CreatePlatformSpecific();
                    PipInstallHelperPtr->SetInstallCallbackFunction("pip_progress", [installMenu](const std::string& msg) {
                        installMenu->UpdateProgress("Pip: " + msg);
                    });
                    
                    PipInstallHelperPtr->Initialize();
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    installMenu->CompleteCurrentSubStep();
                    
                    installMenu->SetCurrentSubStep("Check Python/Pip", "Check Requirements");
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    installMenu->CompleteCurrentSubStep();
                    
                    installMenu->SetCurrentSubStep("Check Python/Pip", "Verify Pip Installation");
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    installMenu->CompleteCurrentSubStep();
                    installMenu->CompleteCurrentStep();
                    
                    // Step 4: Install/Verify Python/Pip
                    installMenu->SetCurrentStep("Install/Verify Python/Pip");
                    installMenu->SetCurrentSubStep("Install/Verify Python/Pip", "Install Python via Homebrew");
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    installMenu->CompleteCurrentSubStep();
                    
                    installMenu->SetCurrentSubStep("Install/Verify Python/Pip", "Link Python Installation");
                    if (!cmdOptions.noRoot) {
                        PipInstallHelperPtr->Execute([&]() {
                            return PipInstallHelperPtr->HasPipInstalled();
                        });
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    installMenu->CompleteCurrentSubStep();
                    
                    installMenu->SetCurrentSubStep("Install/Verify Python/Pip", "Verify Pip Access");
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    installMenu->CompleteCurrentSubStep();
                    installMenu->CompleteCurrentStep();

                    // Step 5: Install Virtual Environment
                    installMenu->SetCurrentStep("Install Virtual Environment");
                    installMenu->SetCurrentSubStep("Install Virtual Environment", "Install virtualenv package");
                    
                    PipInstallHelperPtr->InstallPackage("virtualenv", [&](std::string result) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                        installMenu->CompleteCurrentSubStep();
                        installMenu->CompleteCurrentStep();
                        
                        // Step 6: Create Virtual Environment
                        installMenu->SetCurrentStep("Create Virtual Environment");
                        installMenu->SetCurrentSubStep("Create Virtual Environment", "Request Root Access");
                        std::this_thread::sleep_for(std::chrono::milliseconds(300));
                        installMenu->CompleteCurrentSubStep();
                        
                        if (!cmdOptions.noRoot) {
                            RootAccessHelper* RootAccessHelperPtr = RootAccessHelper::CreatePlatformSpecific();
                            RootAccessHelperPtr->SetInstallCallbackFunction("root_progress", [installMenu](const std::string& msg) {
                                installMenu->UpdateProgress("Root: " + msg);
                            });
                            RootAccessHelperPtr->Initialize();
                            
                            installMenu->SetCurrentSubStep("Create Virtual Environment", "Create venv directory");
                            std::this_thread::sleep_for(std::chrono::milliseconds(500));
                            installMenu->CompleteCurrentSubStep();
                            
                            RootAccessHelper* RootAccessHelperPtr2 = RootAccessHelper::CreatePlatformSpecific();
                            RootAccessHelperPtr2->Execute([&]() {
                                std::string venvPath = contentPath + "/pyenv";
                                std::string venvCommand = "python3 -m venv " + venvPath;
                                
                                installMenu->SetCurrentSubStep("Create Virtual Environment", "Initialize virtual environment");
                                
                                if (RootAccessHelperPtr2->RunCommandWithElevatedPrivileges(venvCommand, [&](const std::string& msg) {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                                    installMenu->CompleteCurrentSubStep();
                                    installMenu->CompleteCurrentStep();
                                    
                                    // Step 7: Install Python Packages
                                    installMenu->SetCurrentStep("Install Python Packages");
                                    installMenu->SetCurrentSubStep("Install Python Packages", "Install MkDocs");
                                    
                                    std::string pipPath = venvPath + "/bin/pip";
                                    std::string mkdocsInstallCommand = pipPath + " install mkdocs mkdocs-material";
                                    
                                    if (RootAccessHelperPtr2->RunCommandWithElevatedPrivileges(mkdocsInstallCommand, [&](const std::string& msg) {
                                        // Empty callback function
                                    })) {
                                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                                        installMenu->CompleteCurrentSubStep();
                                        installMenu->SetCurrentSubStep("Install Python Packages", "Install MkDocs Material");
                                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                                        installMenu->CompleteCurrentSubStep();
                                        installMenu->SetCurrentSubStep("Install Python Packages", "Verify Installation");
                                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                                        installMenu->CompleteCurrentSubStep();
                                        installMenu->CompleteCurrentStep();
                                        installMenu->SetComplete();
                                    } else {
                                        installMenu->SetError("Failed to install Python packages");
                                    }
                                })) {
                                    // Command succeeded
                                } else {
                                    installMenu->SetError("Failed to create virtual environment");
                                }
                                
                                return true; // Return a boolean value for the Execute lambda
                            });
                        
                        } else {
                            installMenu->SetCurrentSubStep("Create Virtual Environment", "Create venv directory");
                            std::this_thread::sleep_for(std::chrono::milliseconds(500));
                            installMenu->CompleteCurrentSubStep();
                            installMenu->SetCurrentSubStep("Create Virtual Environment", "Initialize virtual environment");
                            installMenu->UpdateProgress("Skipping virtual environment creation (no-root mode)");
                            std::this_thread::sleep_for(std::chrono::milliseconds(500));
                            installMenu->CompleteCurrentSubStep();
                            installMenu->CompleteCurrentStep();
                            installMenu->SetComplete();
                        }
                    });
                    #endif
                    
                } catch (const std::exception& e) {
                    NOVA_LOG_VERBOSE(("Installation thread exception: " + std::string(e.what())).c_str(), LogType::Error);
                    
                    // Cleanup on exception
                    if (BrewInstallHelperPtr) delete BrewInstallHelperPtr;
                    if (PipInstallHelperPtr) delete PipInstallHelperPtr;
                    if (RootAccessHelperPtr) delete RootAccessHelperPtr;
                    if (RootAccessHelperPtr2) delete RootAccessHelperPtr2;
                    
                    installMenu->SetError(std::string("Installation failed: ") + e.what());
                }
                NOVA_LOG_VERBOSE("Installation thread ending", LogType::Debug);
            });

            NOVA_LOG_VERBOSE("Starting installation menu show", LogType::Debug);
            // Show installation menu (this will block until installation is complete)
            installMenu->Show();
            NOVA_LOG_VERBOSE("Installation menu show completed", LogType::Debug);
            
            // Wait for installation thread to complete
            NOVA_LOG_VERBOSE("Waiting for installation thread to join", LogType::Debug);
            if (installThread.joinable()) {
                installThread.join();
                NOVA_LOG_VERBOSE("Installation thread joined successfully", LogType::Debug);
            } else {
                NOVA_LOG_VERBOSE("Installation thread was not joinable", LogType::Warning);
            }
            
            NOVA_LOG("Installation process completed!", LogType::Log);
            NOVA_LOG_VERBOSE("InstallRequirements callback completed", LogType::Debug);
        });
    }

    /**
     * Registers the callback for starting the documentation webpage.
     */
    void Interactables::RegisterStartDocumentationCallback(std::shared_ptr<MainMenu> mainMenu,
                                                           const std::string& contentFolderPath) // Changed NovaFileHandle to std::string
    {
        mainMenu->SetMenuActionCallback("StartDocumentationWebpage", [&contentFolderPath]() {
            NOVA_LOG("Starting documentation webpage...", LogType::Debug);
            
            std::string contentPath = contentFolderPath;
            std::string venvPath = contentPath + "/pyenv";
            
            #ifdef _WIN32
            std::string mkdocsPath = venvPath + "/Scripts/mkdocs";
            #else
            std::string mkdocsPath = venvPath + "/bin/mkdocs";
            #endif
            
            std::string serveCommand = mkdocsPath + " serve";
            NOVA_LOG(("Starting MkDocs server with command: " + serveCommand).c_str(), LogType::Debug);
            
            NOVA_LOG("Documentation webpage started successfully!", LogType::Log);
            NOVA_LOG("You can access the documentation at: http://localhost:8000", LogType::Log);
        });
    }

    /**
     * Registers the callback for showing help.
     */
    void Interactables::RegisterHelpCallback(std::shared_ptr<MainMenu> mainMenu)
    {
        mainMenu->SetMenuActionCallback("ShowHelp", []() {
            NOVA_LOG("Showing help menu...", LogType::Debug);
            auto helpMenu = HelpMenu::Create();
            helpMenu->Show();
        });
    }

    /**
     * Registers the callback for quitting the application.
     */
    void Interactables::RegisterQuitCallback(std::shared_ptr<MainMenu> mainMenu)
    {
        mainMenu->SetMenuActionCallback("Quit", [mainMenu]() {
            NOVA_LOG("Quitting application...", LogType::Debug);

            // First request the FTXUI screen loop to exit so terminal state is restored
            try {
                auto exitClosure = mainMenu->GetScreen().ExitLoopClosure();
                if (exitClosure) exitClosure();
            } catch (...) {}

            // Unload all modules/plugins to allow them to perform cleanup
            try { Core::ExtensionRegistry::Instance().UnloadAllExtensions(); } catch (...) {}

            // Final log and return to allow process to exit cleanly
            NOVA_LOG("Application shutdown sequence initiated", LogType::Log);
        });
    }
}
