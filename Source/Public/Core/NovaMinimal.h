#pragma once

// Standard library includes
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <cstdlib>

// Platform-specific includes
#ifdef __APPLE__
#include <unistd.h>
#include <sys/types.h>
#include <Security/Security.h>
#include <Security/AuthorizationTags.h>
#include <ServiceManagement/ServiceManagement.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

// Export/import macros for shared library symbols
#include "Core/ModuleAPI.h"

// Third-party library includes
#include "json.hpp"
#include "ftxui/component/component.hpp"