#pragma once

#include <string>
#include "Core/ModuleAPI.h"

#include "ExtensionSpecific/IExtensionAgent.h"

namespace Core {

enum class EEscalationStatus {
    NotRequired,
    Pending,
    Authenticated,
    Denied
};

struct EscalationHandle {
    EEscalationStatus Status = EEscalationStatus::NotRequired;
    std::string Token; // OS specific or session specific token
};

/**
 * Interface for extensions providing privilege escalation capabilities.
 */
class IPrivilegeEscalationAgent {
public:
    virtual ~IPrivilegeEscalationAgent() = default;

    /**
     * Checks if the current session or process already has elevated privileges.
     */
    virtual bool IsElevated() const = 0;

    /**
     * Returns a handle containing the current escalation status and cached token.
     */
    virtual EscalationHandle GetEscalationHandle() = 0;

    /**
     * Returns the command prefix to use for elevated execution (e.g., "sudo " or "pkexec ").
     */
    virtual std::string GetElevatedCommandPrefix() const = 0;

    /**
     * Returns the menu ID that should be navigated to for user authentication.
     */
    virtual std::string GetEscalationMenuId() const = 0;

    /**
     * Verifies if a given password or token provides elevation.
     * This is used by the escalation menu to confirm credentials.
     */
    virtual bool Authenticate(const std::string& credentials) = 0;

    /**
     * Clears cached elevation credentials.
     */
    virtual void Deauthenticate() = 0;
};

} // namespace Core
