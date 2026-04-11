#pragma once

#include <string>
#include <vector>

namespace Core {

// Descriptor for a single CLI argument that an extension accepts.
struct FExtensionCliArgDescriptor {
    // The argument flag name, e.g. "--rabbitmq-host".
    std::string Flag;
    // Human-readable description shown in --help output.
    std::string Description;
    // Whether the argument requires a value (true) or is a boolean toggle (false).
    bool RequiresValue = true;
};

// A parsed CLI argument matched from the extension's descriptor list.
struct FExtensionCliArg {
    std::string Flag;
    std::string Value; // empty for boolean toggles
};

/**
 * IExtensionCliProvider
 *
 * Extensions implement this interface to declare and consume CLI arguments.
 * The host (CelestiaNova.cpp / CanvasCore CLI dispatch) calls:
 *   1. GetCliArgDescriptors() at startup to know what flags the extension expects.
 *   2. ApplyCliArgs(args) once the extension is loaded, passing only the
 *      arguments that matched its declared descriptors.
 *
 * Extensions do their own deeper parsing (validation, conversion) inside
 * ApplyCliArgs. They must not reach into the global argv directly.
 */
class IExtensionCliProvider {
public:
    virtual ~IExtensionCliProvider() = default;

    // Return the CLI argument descriptors this extension can handle.
    virtual std::vector<FExtensionCliArgDescriptor> GetCliArgDescriptors() const = 0;

    // Called by the host after the extension is loaded and autostarted.
    // `args` contains only the arguments matching this extension's descriptors.
    virtual void ApplyCliArgs(const std::vector<FExtensionCliArg>& args) = 0;
};

} // namespace Core
