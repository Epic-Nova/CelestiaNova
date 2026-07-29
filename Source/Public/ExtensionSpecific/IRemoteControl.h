#pragma once

#include <string>

namespace Core {

// Deliberately narrow bridge for in-process remote-control dispatch.  The
// bearer value exists only for the duration of a dispatch attempt and must
// never be persisted, returned to Canvas, or written to logs.
struct RemoteControlDispatchAuthorization {
    std::string authorizationHeader;
};

// AegisCore is the sole owner of interactive identities and their ephemeral
// bearer sessions.  Consumers may ask it to authorize one typed operation;
// they must never receive a token through Canvas or configuration.
class IAegisSessionCapabilityProvider {
public:
    virtual ~IAegisSessionCapabilityProvider() = default;
    virtual bool HasAuthenticatedAegisSession() const = 0;
    virtual bool AuthorizeRemoteControlDispatch(const std::string& targetId,
                                                const std::string& requiredCapability,
                                                RemoteControlDispatchAuthorization& outAuthorization,
                                                std::string& outError) const = 0;
};

class IRemoteControlTargetResolver {
public:
    virtual ~IRemoteControlTargetResolver() = default;
    virtual bool ResolveAllowlistedRemoteControlRequest(const std::string& targetId,
                                                        const std::string& commandId,
                                                        std::string& outUrl,
                                                        std::string& outMethod,
                                                        std::string& outRequiredCapability,
                                                        std::string& outError) const = 0;
};

} // namespace Core
