/// @file GitInstallHelper_Linux.cpp
/// @brief Linux-specific Git install helper implementation.

#if defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)

#include "Helpers/GitInstallHelper/GitInstallHelper_Linux.h"
#include "Helpers/RootAccessHelper/RootAccessHelper.h"
#include "NovaLog.h"

#include <cstdlib>

namespace Core::Helpers
{
    bool GitInstallHelper_Linux::Initialize()
    {
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_linux.initializing");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_linux.initialized");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_linux.git_installed");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_linux.git_not_installed");

        FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_linux.initializing");

        bIsInitialized = IsGitInstalled();
        if (bIsInitialized)
        {
            FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_linux.git_installed");
        }
        else
        {
            FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_linux.git_not_installed");
        }

        FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_linux.initialized");
        return bIsInitialized;
    }

    void GitInstallHelper_Linux::Shutdown()
    {
        bIsInitialized = false;
        bIsRunning = false;
    }

    void GitInstallHelper_Linux::Execute(std::function<bool()> callback)
    {
        if (bIsRunning)
        {
            return;
        }

        bIsRunning = true;

        if (IsGitInstalled())
        {
            bIsInitialized = true;
            bIsRunning = false;
            if (callback)
            {
                callback();
            }
            return;
        }

        RootAccessHelper* rootAccessHelper = RootAccessHelper::CreatePlatformSpecific();
        if (!rootAccessHelper || !rootAccessHelper->Initialize())
        {
            bIsRunning = false;
            return;
        }

        rootAccessHelper->Execute([&]() {
            bool success = rootAccessHelper->RunCommandWithElevatedPrivileges(
                "apt-get update && apt-get install -y git",
                [](const std::string&) {}
            );

            bIsInitialized = IsGitInstalled();
            bIsRunning = false;
            if (callback)
            {
                callback();
            }
            return success;
        });

        WaitForCompletion();
        delete rootAccessHelper;
    }

    void GitInstallHelper_Linux::Reset()
    {
        bIsRunning = false;
    }

    void GitInstallHelper_Linux::Abort()
    {
        bIsRunning = false;
    }

    bool GitInstallHelper_Linux::HasMetRequirements() const
    {
        return true;
    }

    bool GitInstallHelper_Linux::IsGitInstalled() const
    {
        return (std::system("git --version > /dev/null 2>&1") == 0);
    }

    bool GitInstallHelper_Linux::CloneRepository(const std::string& url, const std::string& destination,
                                                 const std::string& username, const std::string& token)
    {
        if (!IsGitInstalled())
        {
            NOVA_LOG("Git is not installed, cannot clone repository", Core::LogType::Error);
            return false;
        }

        std::string command = "git clone " + url + " " + destination;
        if (!username.empty() || !token.empty())
        {
            command += " --config user.name=" + username + " --config user.password=" + token;
        }

        int result = std::system(command.c_str());
        return (result == 0);
    }
}

#endif // Linux
