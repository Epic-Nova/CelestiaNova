#include "Helpers/RootAccessHelper/Platforms/RootAccessHelper_Windows.h"

#include <functional>
#include <string>

namespace Core::Helpers
{
    bool RootAccessHelper_Windows::Initialize()
    {
        bIsInitialized = true;
        return true;
    }

    void RootAccessHelper_Windows::Shutdown()
    {
        bIsInitialized = false;
    }

    void RootAccessHelper_Windows::Execute(std::function<bool()> callback)
    {
        bIsRunning = true;
        if (callback) callback();
        bIsRunning = false;
    }

    void RootAccessHelper_Windows::Reset()
    {
        bIsRunning = false;
    }

    void RootAccessHelper_Windows::Abort()
    {
        bIsRunning = false;
    }

    bool RootAccessHelper_Windows::HasRootAccess() const
    {
        return false; // Stub: no automatic elevated privileges
    }

    bool RootAccessHelper_Windows::RequestElevatedPrivileges()
    {
        // Not implemented: real implementation should request elevation via ShellExecute/WIN API
        return false;
    }

    std::string RootAccessHelper_Windows::GetElevationCommand(const std::string& command) const
    {
        // Windows elevation not implemented; return command unchanged
        return command;
    }

    bool RootAccessHelper_Windows::RunCommandWithElevatedPrivileges(const std::string& command, std::function<void(std::string)> callback)
    {
        // Fall back to non-elevated execution using BaseHelper helper
        return RunCommandWithProgress(command);
    }

}
