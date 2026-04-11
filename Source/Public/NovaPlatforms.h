/// @file NovaPlatforms.h
/// @brief Platform detection and small compatibility macros.

#pragma once

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
#define NOVA_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
#define NOVA_PLATFORM_DARWIN 1
#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
#define NOVA_PLATFORM_LINUX 1
#else
#error "Unsupported platform"
#endif

// On Windows map POSIX popen/pclose to MSVC names
#ifdef _WIN32
#ifndef popen
#define popen _popen
#endif
#ifndef pclose
#define pclose _pclose
#endif
#endif
