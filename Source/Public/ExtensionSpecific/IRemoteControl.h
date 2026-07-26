#pragma once

#include <string>

namespace Core {

// Deliberately narrow bridge for in-process remote-control dispatch.  The
// bearer value exists only for the duration of a dispatch attempt and must
// never be persisted, returned to Canvas, or written to logs.
struct RemoteControlDispatchAuthorization {
    std::string authorizationHeader;
};

class INovaIdSessionCapabilityProvider {
public:
    virtual ~INovaIdSessionCapabilityProvider() = default;
    virtual bool HasAuthenticatedNovaIdSession() const = 0;
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
