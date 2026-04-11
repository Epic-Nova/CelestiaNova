#pragma once

#include <string>
#include <vector>
#include "Core/ModuleAPI.h"

namespace Core {

// Base status aggregation surface for service HTTP APIs and frontend status pages.
// This class intentionally does not host an HTTP server; it only provides
// normalized payload generation and endpoint discovery for the service layer.
class NOVA_CORE_API StatusApiSurface {
public:
    static std::string BuildExtensionsStatusJson();

    static std::vector<std::string> ListDeclaredHealthEndpoints();

    static std::vector<std::string> ListDeclaredContentEndpoints();

    static std::vector<std::string> ListDeclaredGrafanaDashboards();

    static std::vector<std::string> ListDeclaredServiceCapabilities();

    static std::vector<std::string> ListDeclaredContentPacks();

    static std::vector<std::string> ListDeclaredTelemetryStreams();
};

} // namespace Core
