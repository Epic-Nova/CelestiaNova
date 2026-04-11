#include "Helpers/GitInstallHelper/GitInstallHelper_Windows.h"

#include <cstdlib>
#include <functional>
#include <string>

namespace Core::Helpers
{
    bool GitInstallHelper_Windows::Initialize()
    {
        bIsInitialized = true;
        return true;
    }

    void GitInstallHelper_Windows::Shutdown()
    {
        bIsInitialized = false;
    }

    void GitInstallHelper_Windows::Execute(std::function<bool()> callback)
    {
        bIsRunning = true;
        if (callback) callback();
        bIsRunning = false;
    }

    void GitInstallHelper_Windows::Reset()
    {
        bIsRunning = false;
    }

    void GitInstallHelper_Windows::Abort()
    {
        bIsRunning = false;
    }

    bool GitInstallHelper_Windows::HasMetRequirements() const
    {
        return true;
    }

    bool GitInstallHelper_Windows::IsGitInstalled() const
    {
        int rc = system("git --version >nul 2>nul");
        return rc == 0;
    }

    bool GitInstallHelper_Windows::CloneRepository(const std::string& url, const std::string& destination, const std::string& username, const std::string& token)
    {
        std::string cmd = "git clone \"" + url + "\" \"" + destination + "\"";
        return RunCommandWithProgress(cmd);
    }

}
