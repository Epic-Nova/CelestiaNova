/// @file CommandLineParsing.cpp
/// @brief Command line parsing utilities.

#include "Utils/CommandLineParsing.h"
#include "NovaCore.h"
#include <unordered_set> // Include unordered_set explicitly

namespace Utils
{
    using namespace Core;
    using namespace Utils;

    CommandLineOptionsStruct CommandLineParsing::ParseArguments(int argc, const char* argv[], const std::unordered_map<std::string, std::string*>& optionMapping)
    {
        CommandLineOptionsStruct options;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            // Check if the argument matches any registered option
            for (const auto& [key, value] : optionMapping) {
                if (arg == "-" + key || arg == "--" + key) {
                    *value = true;
                    NOVA_LOG(("Command line: " + key + " enabled").c_str(), LogType::Log);
                    break;
                }
            }

            // Handle help argument
            if (arg == "-help" || arg == "--help" || arg == "-h") {
                DisplayHelp(optionMapping);
                exit(0);
            }
        }

        return options;
    }

    void CommandLineParsing::DisplayHelp(const std::unordered_map<std::string, std::string*>& optionMapping)
    {
        std::cout << "====================================================\n";
        std::cout << " Celestia Nova - Command Line Options\n";
        std::cout << "====================================================\n";
        std::cout << "Usage:\n";
        std::cout << "  celestia_nova [options]\n\n";
        std::cout << "Available Options:\n";

        const int alignmentWidth = 40; // Set a fixed width for alignment

        std::unordered_set<std::string> displayedKeys; // Track displayed formatted keys to avoid duplication

        for (const auto& [key, value] : optionMapping) {
            std::string formattedKey = "-" + key + ", --" + key;

            // Filter out elements starting with a single '-'
            if (formattedKey.find("-") == 0 && formattedKey.find(" ") != std::string::npos) {
                formattedKey = formattedKey.substr(formattedKey.find(" ") + 1); // Remove everything up to the next space
            }

            if (displayedKeys.find(formattedKey) == displayedKeys.end()) { // Only display if not already displayed
                int padding = std::max(0, alignmentWidth - static_cast<int>(formattedKey.length())); // Ensure padding is non-negative
                std::cout << "  " << formattedKey
                          << std::string(padding, ' ') // Dynamically adjust padding
                          << "Enable " << key << " option\n";
                displayedKeys.insert(formattedKey); // Mark the formatted key as displayed
            }
        }

        std::string helpKey = "-help, --help, -h";
        int helpPadding = std::max(0, alignmentWidth - static_cast<int>(helpKey.length())); // Ensure padding is non-negative
        std::cout << "  " << helpKey
                  << std::string(helpPadding, ' ') // Dynamically adjust padding
                  << "Display this help message\n";

        std::cout << "====================================================\n";
        std::cout << std::endl;
    }
}
