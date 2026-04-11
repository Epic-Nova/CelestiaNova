/* ModuleAPI.h
 *
 * Cross-platform export/import macros for the Celestia Nova / FutureLooking
 * extensions. Extensions should follow the example below to define their own
 * MODULE_API macro when building a shared library.
 *
 * Usage (core):
 *   - When building the core library, add `-DNOVA_CORE_EXPORTS` to the
 *     compiler flags. Then `NOVA_CORE_API` will expand to the proper
 *     export decoration.
 *
 * Usage (extension):
 *   - When building an extension named `MyExtension`, add
 *     `-DMyExtension_EXPORTS` to the extension build. In the extension
 *     public headers do:
 *       #include "Core/ModuleAPI.h"
 *       #ifdef MyExtension_EXPORTS
 *       #  define MYEXTENSION_API NOVA_EXPORT
 *       #else
 *       #  define MYEXTENSION_API NOVA_IMPORT
 *       #endif
 *
 *   - Implement a factory function in exactly one translation unit:
 *       #include "Core/ModuleAPI.h"
 *       #include "Core/IExtensionInterface.h"
 *       class MyModule : public IExtensionInterface { ... };
 *       extern "C" MYEXTENSION_API IExtensionInterface* CreateModuleInstance() {
 *           return new MyModule();
 *       }
 *       extern "C" MYEXTENSION_API void DestroyModuleInstance(IExtensionInterface* p) {
 *           delete p;
 *       }
 */

#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
  #if defined(NOVA_STATIC)
    #define NOVA_EXPORT
    #define NOVA_IMPORT
    #define NOVA_LOCAL
  #else
    #define NOVA_EXPORT __declspec(dllexport)
    #define NOVA_IMPORT __declspec(dllimport)
    #define NOVA_LOCAL
  #endif
#else
  #if __GNUC__ >= 4
    #define NOVA_EXPORT __attribute__ ((visibility ("default")))
    #define NOVA_IMPORT __attribute__ ((visibility ("default")))
    #define NOVA_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #define NOVA_EXPORT
    #define NOVA_IMPORT
    #define NOVA_LOCAL
  #endif
#endif

/* Core API macro. Define NOVA_CORE_EXPORTS when building the core library. */
#if defined(NOVA_CORE_EXPORTS)
  #define NOVA_CORE_API NOVA_EXPORT
#else
  #define NOVA_CORE_API NOVA_IMPORT
#endif

/* Convenience: forward declare the extension interface type used by factory
 * helpers so extension headers can reference the type without forcing a
 * circular include. Extension implementations should include
 * Core/IExtensionInterface.h when implementing factories. */
class IExtensionInterface;

/* Helper macro to declare simple exported factory functions. Use this in a
 * single extension .cpp file (do NOT place in a header that may be included
 * by multiple translation units):
 *
 *   // in MyExtension.cpp
 *   #include "Core/ModuleAPI.h"
 *   #include "PublicHeadersForMyExtension.h"
 *   class MyModule : public IExtensionInterface { ... };
 *   NOVA_DECLARE_MODULE_FACTORY(MYEXTENSION_API, MyModule)
 *
 */
#define NOVA_DECLARE_MODULE_FACTORY(API, CLASS) \
  extern "C" API IExtensionInterface* CreateModuleInstance() { return new CLASS(); } \
  extern "C" API void DestroyModuleInstance(IExtensionInterface* p) { delete p; }
