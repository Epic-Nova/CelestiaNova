/// @file PipInstallHelper_Linux.cpp
/// @brief Linux-specific pip install helper implementation.

#if defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)

#include "Helpers/PipInstallHelper/Platforms/PipInstallHelper_Linux.h"
#include "Helpers/RootAccessHelper/RootAccessHelper.h"
#include "NovaCore.h"

#include <cstdlib>

namespace Core::Helpers
{
    bool PipInstallHelper_Linux::Initialize()
    {
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.pip_install_helper_linux.initializing");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.pip_install_helper_linux.pip_installed");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.pip_install_helper_linux.pip_not_installed");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.pip_install_helper_linux.initialized");

        FireInstallCallback("com.epicnova.adi.fh.ds.pip_install_helper_linux.initializing");

        bIsInitialized = HasPipInstalled();
        if (bIsInitialized)
        {
            FireInstallCallback("com.epicnova.adi.fh.ds.pip_install_helper_linux.pip_installed");
        }
        else
        {
            FireInstallCallback("com.epicnova.adi.fh.ds.pip_install_helper_linux.pip_not_installed");
        }

        FireInstallCallback("com.epicnova.adi.fh.ds.pip_install_helper_linux.initialized");
        return bIsInitialized;
    }

    void PipInstallHelper_Linux::Shutdown()
    {
        bIsInitialized = false;
        bIsRunning = false;
    }

    void PipInstallHelper_Linux::Execute(std::function<bool()> callback)
    {
        bIsRunning = true;

        if (HasPipInstalled())
        {
            bIsInitialized = true;
            bIsRunning = false;
            if (callback)
            {
                callback();
            }
            return;
        }

        if (!HasMetRequirements())
        {
            bIsRunning = false;
            return;
        }

        RootAccessHelper* rootAccessHelper = RootAccessHelper::CreatePlatformSpecific();
        if (!rootAccessHelper || !rootAccessHelper->Initialize())
        {
            bIsRunning = false;
            return;
        }

        rootAccessHelper->Execute([&]() {
            bool success = ExecuteCommandBlocking("python3 -m ensurepip --upgrade");
            if (!success)
            {
                success = rootAccessHelper->RunCommandWithElevatedPrivileges(
                    "apt-get update && apt-get install -y python3-pip",
                    [](const std::string&) {}
                );
            }

            bIsInitialized = HasPipInstalled();
            if (callback)
            {
                callback();
            }
            bIsRunning = false;
            return success;
        });

        WaitForCompletion();
        delete rootAccessHelper;
    }

    void PipInstallHelper_Linux::Reset()
    {
        bIsRunning = false;
        bIsInitialized = false;
    }

    void PipInstallHelper_Linux::Abort()
    {
        bIsRunning = false;
    }

    bool PipInstallHelper_Linux::HasMetRequirements() const
    {
        return (std::system("python3 --version > /dev/null 2>&1") == 0);
    }

    bool PipInstallHelper_Linux::HasPipInstalled() const
    {
        if (std::system("pip --version > /dev/null 2>&1") == 0)
        {
            return true;
        }
        if (std::system("pip3 --version > /dev/null 2>&1") == 0)
        {
            return true;
        }
        if (std::system("python3 -m pip --version > /dev/null 2>&1") == 0)
        {
            return true;
        }
        return false;
    }

    void PipInstallHelper_Linux::InstallPackage(const std::string& packageName,
                                                std::function<void(std::string)> callback)
    {
        if (!HasPipInstalled())
        {
            if (callback)
            {
                callback("Pip is not installed.");
            }
            return;
        }

        bIsRunning = true;

        std::string command = "python3 -m pip install " + packageName;
        bool success = ExecuteCommandBlocking(command);

        if (callback)
        {
            callback(success ? ("Package installed: " + packageName)
                             : ("Failed to install package: " + packageName));
        }

        bIsRunning = false;
    }
}

#endif // Linux
