/// @file PipInstallHelper_Windows.h
/// @brief Windows-specific pip helper (stub).

#pragma once

#ifdef _WIN32

#include "Helpers/PipInstallHelper/PipInstallHelper.h"

namespace Core::Helpers
{
    class PipInstallHelper_Windows : public PipInstallHelper
    {
    public:
        virtual bool Initialize() override;
        virtual void Shutdown() override;
        virtual void Execute(std::function<bool()> callback) override;
        virtual void Reset() override;
        virtual void Abort() override;
        virtual bool HasMetRequirements() const override;
        virtual void InstallPackage(const std::string& packageName, std::function<void(std::string)> callback) override;
    };
}

#endif // _WIN32
