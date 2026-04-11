/// @file GitInstallHelper_Linux.h
/// @brief Linux-specific Git install helper.

#pragma once

#if defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)

#include "Helpers/GitInstallHelper/GitInstallHelper.h"

namespace Core::Helpers
{
    class GitInstallHelper_Linux : public GitInstallHelper
    {
    public:
        virtual bool Initialize() override;
        virtual void Shutdown() override;
        virtual void Execute(std::function<bool()> callback) override;
        virtual void Reset() override;
        virtual void Abort() override;
        virtual bool HasMetRequirements() const override;
        virtual bool IsGitInstalled() const override;
        virtual bool CloneRepository(const std::string& url, const std::string& destination,
                                     const std::string& username = "", const std::string& token = "") override;
    };
}

#endif // Linux
