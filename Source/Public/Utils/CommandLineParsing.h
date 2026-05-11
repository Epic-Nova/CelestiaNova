#pragma once

#include "NovaMinimal.h"
#include "Utils/CommandLineOptions.h"
#include "Core/ModuleAPI.h"
#include "ExtensionSpecific/IExtensionCliProvider.h"

namespace Utils
{
    /**
     * Utility class for parsing command line arguments.
     */
    class NOVA_CORE_API CommandLineParsing
    {
    public:
        /**
         * Parse command line arguments into options.
         * 
         * @param argc The argument count
         * @param argv The argument values
         * @return The parsed command line options
         */
        static CommandLineOptionsStruct ParseArguments(int argc, const char* argv[], 
                                                     const std::unordered_map<std::string, std::string*>& optionMapping,
                                                     const std::unordered_map<std::string, bool*>& boolMapping = {});
        
        /**
         * Parses arguments specifically for an extension based on its descriptors.
         */
        static std::vector<Core::FExtensionCliArg> ParseExtensionArguments(int argc, const char* argv[], const std::vector<Core::FExtensionCliArgDescriptor>& descriptors);

        /**
         * Display help information about command line options.
         */
        static void DisplayHelp(const std::unordered_map<std::string, std::string*>& optionMapping);
    };
}
