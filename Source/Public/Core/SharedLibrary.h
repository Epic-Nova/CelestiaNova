#pragma once

#include <string>
#include <optional>

class SharedLibrary {
public:
    SharedLibrary() = default;
    ~SharedLibrary();

    // Load the shared library from the given path. On failure, returns false
    // and sets `err` with a platform-specific message if provided.
    bool Load(const std::string& path, std::string* err = nullptr);

    // Unload the library if loaded.
    void Unload();

    // Returns a symbol pointer or nullptr if not found.
    void* GetSymbol(const std::string& name) const;

    bool IsLoaded() const;

    // Helper: returns true if the filename looks like a shared library on
    // the current platform (.dll, .so, .dylib, .bundle).
    static bool IsLibraryFile(const std::string& filename);

private:
    void* handle_ = nullptr;
};
