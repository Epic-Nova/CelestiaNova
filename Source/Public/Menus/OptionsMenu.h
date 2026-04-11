#pragma once

#include "BaseMenu.h"
#include "Utils/CommandLineParsing.h"
#include "Utils/CommandLineOptions.h"
#include <vector>
#include <string>
#include <unordered_map>
#include "Core/ModuleAPI.h"

namespace Menus
{
    using namespace ftxui;
    using Utils::CommandLineOptions;
    using Utils::CommandLineOptionsStruct;

    class NOVA_CORE_API OptionsMenu : public BaseMenu
    {
    public:
        static std::shared_ptr<OptionsMenu> Create(CommandLineOptionsStruct& options);

        static OptionsMenu* GetSingletonInstance() { return SingletonPtr; }
        
        void Show() override;
        void Hide() override;
        virtual ~OptionsMenu() = default;

        void LoadConfig();
        void SaveConfig();

        void RegisterCommandLineOptions();
        void ApplyCommandLineOptions(const CommandLineOptionsStruct& cmdOptions);
        const std::unordered_map<std::string, std::string*>& GetCommandLineMapping() const { return commandLineMapping_; }

        CommandLineOptionsStruct& options_;
    private:
        std::unordered_map<std::string, std::string*> commandLineMapping_;
    
    OptionsMenu(CommandLineOptionsStruct& options) : options_(options) {}

        CommandLineOptions* optionManager_ = Utils::CommandLineOptions::GetSingletonInstance();

        static OptionsMenu* SingletonPtr;
    };
}
