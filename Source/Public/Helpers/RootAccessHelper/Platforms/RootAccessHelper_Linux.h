/// @file RootAccessHelper_Linux.h
/// @brief Linux-specific root/elevated access helper.

#pragma once

#if defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)

#include "Helpers/RootAccessHelper/RootAccessHelper.h"

namespace Core::Helpers
{
    class RootAccessHelper_Linux : public RootAccessHelper
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

#endif // Linux
