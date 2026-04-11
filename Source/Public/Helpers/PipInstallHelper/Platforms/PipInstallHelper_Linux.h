/// @file PipInstallHelper_Linux.h
/// @brief Linux-specific pip install helper.

#pragma once

#if defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)

#include "Helpers/PipInstallHelper/PipInstallHelper.h"

namespace Core::Helpers
{
	class PipInstallHelper_Linux : public PipInstallHelper
	{
	public:
		PipInstallHelper_Linux() : PipInstallHelper() {}

		virtual bool Initialize() override;
		virtual void Shutdown() override;
		virtual void Execute(std::function<bool()> callback) override;
		virtual void Reset() override;
		virtual void Abort() override;
		virtual bool HasMetRequirements() const override;
		virtual bool HasPipInstalled() const override;
		virtual void InstallPackage(const std::string& packageName, std::function<void(std::string)> callback) override;
	};
}

#endif // Linux
