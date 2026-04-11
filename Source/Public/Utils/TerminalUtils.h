#pragma once

#include "NovaMinimal.h"
#include "Core/ModuleAPI.h"

namespace Utils
{
    /**
     * Utility class for terminal window management.
     */
    class NOVA_CORE_API TerminalUtils
    {
    public:
        /**
         * Sets the size of the terminal window.
         * @param width The desired width in columns
         * @param height The desired height in rows
         * @return True if operation was successful, false otherwise
         */
        static bool SetTerminalSize(int width, int height);
        
        /**
         * Gets the current terminal window size.
         * @param width Reference to store the current width
         * @param height Reference to store the current height
         * @return True if operation was successful, false otherwise
         */
        static bool GetTerminalSize(int& width, int& height);
    };
}
