/// @file RootAccessHelper_Windows.h
/// @brief Windows-specific root/elevated access helper (stubbed).

#pragma once

#ifdef _WIN32

#include "Helpers/RootAccessHelper/RootAccessHelper.h"

namespace Core::Helpers
{
    class RootAccessHelper_Windows : public RootAccessHelper
    {
    public:
        virtual bool Initialize() override;
        virtual void Shutdown() override;
        virtual void Execute(std::function<bool()> callback) override;
        virtual void Reset() override;
        virtual void Abort() override;
        virtual bool HasRootAccess() const override;

        virtual bool RequestElevatedPrivileges() override;
        virtual std::string GetElevationCommand(const std::string& command) const override;
        virtual bool RunCommandWithElevatedPrivileges(const std::string& command,
                                                      std::function<void(std::string)> callback) override;
    };
}

#endif // _WIN32
