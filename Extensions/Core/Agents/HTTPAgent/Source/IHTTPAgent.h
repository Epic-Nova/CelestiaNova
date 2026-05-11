#pragma once

#include <string>
#include <map>
#include "Core/ModuleAPI.h"
#include "ExtensionSpecific/IExtensionAgent.h"

namespace Core {

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
};

} // namespace Core
