#pragma once

#include "NovaMinimal.h"
#include "Menus/BaseMenu.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include "Core/ModuleAPI.h"

namespace Menus
{
    /**
     * Main menu for the application.
     */
    class NOVA_CORE_API MainMenu : public BaseMenu
    {
    public:
        /**
         * Create an instance of the main menu.
         */
        static std::shared_ptr<MainMenu> InitializeMainMenu();
        
        /**
         * Display the main menu and handle input.
         */
        virtual void Show() override;
        
        /**
         * Hide the main menu.
         */
        virtual void Hide() override;
        
    private:
        
        /**
         * Wrap a component in a window with a name.
         */
        ftxui::Component Wrap(std::string name, ftxui::Component component);
    };
}