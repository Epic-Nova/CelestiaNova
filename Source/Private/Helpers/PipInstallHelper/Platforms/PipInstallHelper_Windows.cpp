#include "Helpers/PipInstallHelper/Platforms/PipInstallHelper_Windows.h"

#include <cstdlib>
#include <functional>
#include <string>

namespace Core::Helpers
{
    bool PipInstallHelper_Windows::Initialize()
    {
        bIsInitialized = true;
        return true;
    }

    void PipInstallHelper_Windows::Shutdown()
    {
        bIsInitialized = false;
    }

    void PipInstallHelper_Windows::Execute(std::function<bool()> callback)
    {
        bIsRunning = true;
        if (callback) callback();
        bIsRunning = false;
    }

    void PipInstallHelper_Windows::Reset()
    {
        bIsRunning = false;
    }

    void PipInstallHelper_Windows::Abort()
    {
        bIsRunning = false;
    }

    bool PipInstallHelper_Windows::HasMetRequirements() const
    {
        int rc = system("python --version >nul 2>nul");
        return rc == 0;
    }

    void PipInstallHelper_Windows::InstallPackage(const std::string& packageName, std::function<void(std::string)> callback)
    {
        std::string cmd = "python -m pip install " + packageName;
        RunCommandWithProgress(cmd);
        if (callback) callback("installed");
    }

}
