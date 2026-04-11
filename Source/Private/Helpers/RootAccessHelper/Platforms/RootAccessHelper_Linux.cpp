/// @file RootAccessHelper_Linux.cpp
/// @brief Linux-specific root/elevated access implementation.

#if defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)

#include "Helpers/RootAccessHelper/Platforms/RootAccessHelper_Linux.h"
#include "NovaCore.h"

#include <unistd.h>
#include <cstdlib>
#include <cstdio>

namespace Core::Helpers
{
	bool RootAccessHelper_Linux::Initialize()
	{
		RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.root_access_helper_linux.initializing");
		RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.root_access_helper_linux.initialized");
		RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.root_access_helper_linux.not_initialized");
		RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.root_access_helper_linux.elevated_privileges_granted");
		RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.root_access_helper_linux.elevated_privileges_failed");
		RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.root_access_helper_linux.command_starting");
		RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.root_access_helper_linux.command_success");
		RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.root_access_helper_linux.command_failed");

		FireInstallCallback("com.epicnova.adi.fh.ds.root_access_helper_linux.initializing");

		bIsInitialized = true;
		bHasElevatedPrivileges = HasRootAccess();

		FireInstallCallback("com.epicnova.adi.fh.ds.root_access_helper_linux.initialized");
		return bIsInitialized;
	}

	void RootAccessHelper_Linux::Shutdown()
	{
		bIsInitialized = false;
		bHasElevatedPrivileges = false;
	}

	void RootAccessHelper_Linux::Execute(std::function<bool()> callback)
	{
		if (!bIsInitialized)
		{
			FireInstallCallback("com.epicnova.adi.fh.ds.root_access_helper_linux.not_initialized");
			return;
		}

		if (!RequestElevatedPrivileges())
		{
			FireInstallCallback("com.epicnova.adi.fh.ds.root_access_helper_linux.elevated_privileges_failed");
			return;
		}

		bIsRunning = true;
		FireInstallCallback("com.epicnova.adi.fh.ds.root_access_helper_linux.elevated_privileges_granted");

		if (callback)
		{
			callback();
		}

		bIsRunning = false;
	}

	void RootAccessHelper_Linux::Reset()
	{
		bIsRunning = false;
		bHasElevatedPrivileges = HasRootAccess();
	}

	void RootAccessHelper_Linux::Abort()
	{
		bIsRunning = false;
	}

	bool RootAccessHelper_Linux::HasRootAccess() const
	{
		return (geteuid() == 0) || bHasElevatedPrivileges;
	}

	bool RootAccessHelper_Linux::RequestElevatedPrivileges()
	{
		if (HasRootAccess())
		{
			bHasElevatedPrivileges = true;
			return true;
		}

		int result = std::system("sudo -v > /dev/null 2>&1");
		bHasElevatedPrivileges = (result == 0);
		return bHasElevatedPrivileges;
	}

	std::string RootAccessHelper_Linux::GetElevationCommand(const std::string& command) const
	{
		return "sudo " + command;
	}

	bool RootAccessHelper_Linux::RunCommandWithElevatedPrivileges(
		const std::string& command,
		std::function<void(std::string)> callback)
	{
		if (!RequestElevatedPrivileges())
		{
			return false;
		}

		FireInstallCallback("com.epicnova.adi.fh.ds.root_access_helper_linux.command_starting");

		std::string fullCommand = GetElevationCommand(command) + " 2>&1";
		FILE* pipe = popen(fullCommand.c_str(), "r");
		if (!pipe)
		{
			FireInstallCallback("com.epicnova.adi.fh.ds.root_access_helper_linux.command_failed");
			return false;
		}

		char buffer[256];
		while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
		{
			if (callback)
			{
				callback(std::string(buffer));
			}
		}

		int returnCode = pclose(pipe);
		if (returnCode == 0)
		{
			FireInstallCallback("com.epicnova.adi.fh.ds.root_access_helper_linux.command_success");
			return true;
		}

		FireInstallCallback("com.epicnova.adi.fh.ds.root_access_helper_linux.command_failed");
		return false;
	}
}

#endif // Linux
