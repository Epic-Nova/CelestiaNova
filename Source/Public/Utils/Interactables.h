#pragma once

#include "NovaMinimal.h"
#include "Utils/CommandLineOptions.h"
#include "Core/ModuleAPI.h"

// Forward declarations
namespace Menus {
    class MainMenu;
}

namespace Utils
{
    using namespace Menus;

    /**
     * Utility class for registering menu callbacks.
     *
     * @deprecated Superseded by metadata-driven CanvasCore actions and the
     *   IExtensionCliProvider pattern. Do not add new callbacks here.
     *   Remove after legacy menus (MainMenu et al.) are fully retired.
     */
    class [[deprecated("Superseded by metadata-driven CanvasCore actions. "
                       "Remove after legacy menus are fully retired.")]]
    NOVA_CORE_API Interactables
    {
    public:
        /**
         * Register all menu callbacks.
         * @deprecated See class deprecation notice.
         */
        [[deprecated("Use CanvasCore menu definitions and IExtensionCliProvider instead.")]]
        static void RegisterMenuCallbacks(std::shared_ptr<MainMenu> mainMenu,
                                          CommandLineOptionsStruct cmdOptions,
                                          const std::string& contentFolderPath);

    private:
        [[deprecated]] static void RegisterInstallRequirementsCallback(std::shared_ptr<MainMenu> mainMenu,
                                                       CommandLineOptionsStruct cmdOptions,
                                                       const std::string& contentFolderPath);
        [[deprecated]] static void RegisterExtensionsCallback(std::shared_ptr<MainMenu> mainMenu,
                               CommandLineOptionsStruct cmdOptions,
                               const std::string& contentFolderPath);

        [[deprecated]] static void RegisterOptionsCallback(std::shared_ptr<MainMenu> mainMenu,
                                            CommandLineOptionsStruct cmdOptions);

        [[deprecated]] static void RegisterStartDocumentationCallback(std::shared_ptr<MainMenu> mainMenu,
                                                      const std::string& contentFolderPath);

        [[deprecated]] static void RegisterHelpCallback(std::shared_ptr<MainMenu> mainMenu);

        [[deprecated]] static void RegisterQuitCallback(std::shared_ptr<MainMenu> mainMenu);
    };
}
