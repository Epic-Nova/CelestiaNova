#include "Core/SharedLibrary.h"
#include <string>
#include <cstring>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dlfcn.h>
#  include <libgen.h>
#endif

bool SharedLibrary::Load(const std::string& path, std::string* err) {
#if defined(_WIN32)
    HMODULE h = LoadLibraryA(path.c_str());
    if (!h) {
        if (err) *err = "LoadLibraryA failed";
        return false;
    }
    handle_ = reinterpret_cast<void*>(h);
    return true;
#else
    // RTLD_NOW to resolve symbols immediately and RTLD_LOCAL to avoid
    // exporting symbols to subsequently loaded libs
    void* h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        if (err) *err = dlerror();
        return false;
    }
    handle_ = h;
    return true;
#endif
}

void SharedLibrary::Unload() {
    if (!handle_) return;
#if defined(_WIN32)
    FreeLibrary(reinterpret_cast<HMODULE>(handle_));
#else
    dlclose(handle_);
#endif
    handle_ = nullptr;
}

void* SharedLibrary::GetSymbol(const std::string& name) const {
    if (!handle_) return nullptr;
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(handle_), name.c_str()));
#else
    return dlsym(handle_, name.c_str());
#endif
}

SharedLibrary::~SharedLibrary() { Unload(); }

bool SharedLibrary::IsLoaded() const { return handle_ != nullptr; }

static std::string ToLower(std::string s) {
    for (auto &c : s) c = static_cast<char>(std::tolower((unsigned char)c));
    return s;
}

bool SharedLibrary::IsLibraryFile(const std::string& filename) {
    std::string name = filename;
    // extract extension
    auto pos = name.find_last_of('.');
    std::string ext;
    if (pos != std::string::npos) ext = ToLower(name.substr(pos));
#if defined(_WIN32)
    return ext == ".dll";
#elif defined(__APPLE__)
    return ext == ".dylib" || ext == ".so" || ext == ".bundle";
#else
    return ext == ".so" || ext == ".dylib";
#endif
}
