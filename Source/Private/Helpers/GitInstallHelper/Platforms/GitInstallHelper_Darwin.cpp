#ifdef __APPLE__

#include "Helpers/GitInstallHelper/GitInstallHelper_Darwin.h"
#include "Helpers/BrewInstallHelper.h"
#include "Helpers/RootAccessHelper/RootAccessHelper.h"
#include "NovaLog.h"
#include <memory>
#include <cstdlib>

namespace Core::Helpers
{
    bool GitInstallHelper_Darwin::Initialize()
    {
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.initializing");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.git_already_installed");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.git_not_installed");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.initialized");
 
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.brew_not_initialized");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.brew_not_installed");

        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.root_access_not_initialized");

        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.uninstalling_git");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.git_uninstalled");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.shutdown_completed");

        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.already_running");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.execute");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.not_initialized");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.installing_git");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.git_install_failed");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.git_installed");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.cloning_without_authentication");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.cloned_successfully");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.clone_failed");

        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.reset");
        RegisterEmptyInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.aborted");


        NOVA_LOG_VERBOSE("GitInstallHelper_Darwin initializing...", Core::LogType::Debug);
        FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.initializing");
        
        bIsInitialized = IsGitInstalled();

        if (bIsInitialized) 
        {
            NOVA_LOG("Git is already installed on macOS", Core::LogType::Log);
            FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.git_already_installed");
        } 
        else 
        {
            NOVA_LOG("Git is not installed, will need to install it", Core::LogType::Log);
            FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.git_not_installed");
        }
        
        NOVA_LOG_VERBOSE("GitInstallHelper_Darwin initialized successfully", Core::LogType::Debug);
        FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.initialized");

        return bIsInitialized;
    }

    void GitInstallHelper_Darwin::Shutdown()
    {
        NOVA_LOG_VERBOSE("GitInstallHelper_Darwin shutting down...", Core::LogType::Debug);
        FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.shutdown");
        
        if (bIsInitialized)
        {
            BrewInstallHelper* brewHelper = BrewInstallHelper::CreatePlatformSpecific();
            if(!brewHelper->Initialize())
            {
                NOVA_LOG("Failed to initialize BrewInstallHelper", Core::LogType::Error);
                FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.brew_not_initialized");
                return;
            }
            
            RootAccessHelper* rootHelper = RootAccessHelper::CreatePlatformSpecific();
            if (!rootHelper->Initialize())
            {
                NOVA_LOG("Failed to initialize RootAccessHelper", Core::LogType::Error);
                FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.root_access_not_initialized");
                return;
            }

            if (brewHelper->IsBrewInstalled() && rootHelper->HasRootAccess())
            {
                NOVA_LOG("Uninstalling Git using Homebrew", Core::LogType::Log);
                FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.uninstalling_git");

                rootHelper->RunCommandWithElevatedPrivileges("brew uninstall git", [](const std::string& msg) {
                    // Empty callback
                });
                
                FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.git_uninstalled");
                NOVA_LOG("Git has been uninstalled successfully", Core::LogType::Log);
            }
            else
            {
                NOVA_LOG("Homebrew is not installed, cannot uninstall Git", Core::LogType::Warning);
                FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.brew_not_installed");
            }
        }

        Reset();
        NOVA_LOG_VERBOSE("GitInstallHelper_Darwin shutdown completed", Core::LogType::Debug);
        FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.shutdown_completed");
    }

    void GitInstallHelper_Darwin::Execute(std::function<bool()> callback)
    {
        if (bIsRunning)
        {
            NOVA_LOG("Git installation process is already running", Core::LogType::Warning);
            FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.already_running");
            return;
        }
        
        bIsRunning = true;

        NOVA_LOG("Executing Git installation process", Core::LogType::Debug);
        FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.execute");
        
        if (!bIsInitialized)
        {
            NOVA_LOG("GitInstallHelper_Darwin is not initialized", Core::LogType::Error);
            FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.not_initialized");
            return;
        }

        // Check if Git is installed
        if (IsGitInstalled())
        {
            NOVA_LOG("Git is already installed, skipping installation", Core::LogType::Log);
            FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.git_already_installed");
            if (callback) callback();
            return;
        }

        // Install Git using Homebrew
        BrewInstallHelper* brewHelper = BrewInstallHelper::CreatePlatformSpecific();
        if(!brewHelper->Initialize())  // Fixed: removed semicolon
        {
            NOVA_LOG("Failed to initialize BrewInstallHelper", Core::LogType::Error);
            FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.brew_not_initialized");
            return;
        }

        if (brewHelper->IsBrewInstalled())
        {
            NOVA_LOG("Installing Git using Homebrew", Core::LogType::Log);
            FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.installing_git");

            if(brewHelper->IsBrewInstalled())
            {
                // Fixed: Capture callback in the lambda
                brewHelper->InstallPackage("git", [this, callback](bool success) {
                    if (success)
                    {
                        bIsInitialized = true;
                        FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.git_installed");
                        NOVA_LOG("Git has been installed successfully", Core::LogType::Log);
                        if (callback) callback();
                    }
                    else
                    {
                        FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.git_install_failed");
                        NOVA_LOG("Failed to install Git", Core::LogType::Error);
                    }
                    bIsRunning = false;
                });
            }
        }
        else
        {
            FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.brew_not_installed");
            NOVA_LOG("Homebrew is not installed, cannot install Git", Core::LogType::Error);
            bIsRunning = false;
        }

        WaitForCompletion();

        delete brewHelper;
    }

    void GitInstallHelper_Darwin::Reset()
    {
        NOVA_LOG("Resetting GitInstallHelper_Darwin", Core::LogType::Debug);
        bIsRunning = false;
        FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.reset");
    }

    void GitInstallHelper_Darwin::Abort()
    {
        NOVA_LOG("Aborting Git installation process", Core::LogType::Debug);
        bIsInitialized = false;
        bIsRunning = false;
        FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.aborted");
    }

    bool GitInstallHelper_Darwin::HasMetRequirements() const
    {
        BrewInstallHelper* brewHelper = BrewInstallHelper::CreatePlatformSpecific();
        brewHelper->Initialize();
        bool result = brewHelper->IsBrewInstalled();
        delete brewHelper;
        return result;
    }

    // Fixed: Made FireInstallCallback calls non-const by storing status locally
    bool GitInstallHelper_Darwin::IsGitInstalled() const
    {
        // Check if git is installed by trying to run it
        int result = std::system("git --version > /dev/null 2>&1");
        bool isInstalled = (result == 0);
        
        if (isInstalled) 
        {
            NOVA_LOG("Git is installed and accessible.", Core::LogType::Debug);
            // Can't call FireInstallCallback in a const method
        }
        else
        {
            NOVA_LOG("Git is not installed or not accessible.", Core::LogType::Debug);
            // Can't call FireInstallCallback in a const method
        }
        
        return isInstalled;
    }

    bool GitInstallHelper_Darwin::CloneRepository(const std::string& url, const std::string& destination,
                                                  const std::string& username, const std::string& token)
    {
        if (!IsGitInstalled()) 
        {
            NOVA_LOG("Git is not installed, cannot clone repository", Core::LogType::Error);
            FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.git_not_installed");
            return false;
        }

        std::string command = "git clone " + url + " " + destination;

        if (username.empty() && token.empty()) 
        {
            NOVA_LOG("No credentials provided, cloning without authentication", Core::LogType::Warning);
            FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.cloning_without_authentication");
        } 
        
        command += " --config user.name=" + username + " --config user.password=" + token;

        int result = std::system(command.c_str());
        bool cloneSuccess = (result == 0);
        
        if (cloneSuccess) 
        {
            NOVA_LOG("Repository cloned successfully", Core::LogType::Log);
            FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.cloned_successfully");
        } 
        else 
        {
            NOVA_LOG("Failed to clone repository", Core::LogType::Error);
            FireInstallCallback("com.epicnova.adi.fh.ds.git_install_helper_darwin.clone_failed");
        }
        
        return cloneSuccess;
    }
}

#endif
