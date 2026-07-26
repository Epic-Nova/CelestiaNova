#pragma once

#include <string>
#include <map>
#include <functional>
#include "Core/ModuleAPI.h"
#include "ExtensionSpecific/IExtensionAgent.h"

namespace Core {

// Deliberately small transport contract for privileged in-process callers.
// Headers are transient request material: implementations must never persist
// or log them (in particular, Authorization).
struct SecureHttpsRequest {
    std::string url;
    std::string method;
    std::string body;
    std::map<std::string, std::string> headers;
    unsigned int timeoutMs = 10000;
    size_t maxResponseBytes = 65536;
};

struct SecureHttpsResponse {
    bool transportSucceeded = false;
    unsigned long statusCode = 0;
    std::string body;
    std::string error; // Sanitized transport error; never includes headers.
};

/**
 * Interface for HTTP operations.
 */
class IHTTPAgent : public virtual IExtensionAgent {
public:
    virtual ~IHTTPAgent() = default;

    /**
     * Performs an HTTP GET request.
     */
    virtual std::string Get(const std::string& url, const std::map<std::string, std::string>& headers = {}) = 0;

    /**
     * Performs an HTTP POST request.
     */
    virtual std::string Post(const std::string& url, const std::string& body, const std::map<std::string, std::string>& headers = {}) = 0;

    /**
     * Downloads a file from a URL.
     */
    virtual bool DownloadFile(const std::string& url, const std::string& destinationPath) = 0;

    // Executes only HTTPS requests and returns immediately.  The callback is
    // invoked exactly once on a worker thread.  This is intentionally not a
    // generic curl bridge so bearer credentials cannot reach shell history.
    virtual std::string DispatchSecureHttpsAsync(const SecureHttpsRequest& request,
                                                 std::function<void(SecureHttpsResponse)> callback) = 0;
};

} // namespace Core
