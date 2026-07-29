#include "Core/ExtensionRegistry.h"
#include <filesystem>
#include <functional>
#include <unordered_set>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "Core/SharedLibrary.h"
#include "Core/NovaLog.h"
#include "Core/NovaFileOperations.h"
#include "json.hpp"
#include "Utils/CommandLineParsing.h"
#include "ExtensionSpecific/IExtensionCliProvider.h"

using namespace Core;
namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

static std::string NowUtcIso8601() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm gmt{};
#if defined(_WIN32)
    gmtime_s(&gmt, &tt);
#else
    gmt = *std::gmtime(&tt);
#endif
    std::ostringstream oss;
    oss << std::put_time(&gmt, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace

static ExtensionDescriptor ParseDescriptorFile(const fs::path& path) {
    ExtensionDescriptor desc;
    try {
        std::string body = Core::FileOperations::NovaFileOperations::ReadTextFile(path.string());
        if (body.empty()) {
            NOVA_LOG(("ExtensionRegistry: empty descriptor " + path.string()).c_str(), LogType::Warning);
            return desc;
        }

        json parsed = json::parse(body);
        desc.id = parsed.value("id", "");
        desc.name = parsed.value("name", "");
        desc.description = parsed.value("description", "");
        desc.longDescription = parsed.value("longDescription", "");
        desc.file = parsed.value("file", "");
        desc.version = parsed.value("version", "");
        desc.autostart = parsed.value("autostart", true);
        desc.startupDelayMs = parsed.value("startupDelayMs", 0);

        auto parseStringArray = [&](const char* key, std::vector<std::string>& out) {
            if (!parsed.contains(key) || !parsed[key].is_array()) {
                return;
            }
            for (const auto& item : parsed[key]) {
                if (!item.is_string()) {
                    continue;
                }
                std::string value = item.get<std::string>();
                if (!value.empty()) {
                    out.push_back(value);
                }
            }
        };

        parseStringArray("dependencies", desc.dependencies);
        parseStringArray("extensionDependencies", desc.extensionDependencies);
        parseStringArray("publicIncludePaths", desc.publicIncludePaths);
    } catch (const std::exception& e) {
        NOVA_LOG(("ExtensionRegistry: failed to parse descriptor " + path.string() + ": " + e.what()).c_str(), LogType::Error);
    }
    return desc;
}

ExtensionRegistry& ExtensionRegistry::Instance() {
    static ExtensionRegistry inst;
    return inst;
}

int ExtensionRegistry::Discover(const std::string& extensionsDir) {
    int count = 0;
    try {
        std::vector<std::string> candidates;
        candidates.push_back(extensionsDir);

#ifdef PROJECT_SOURCE_DIR
        candidates.push_back(std::string(PROJECT_SOURCE_DIR) + "/" + extensionsDir);
#endif
        try {
            auto parent = std::filesystem::current_path().parent_path().string();
            candidates.push_back(parent + "/" + extensionsDir);
        } catch (...) {}

        std::string resolved;
        for (const auto& cand : candidates) {
            if (cand.empty()) continue;
            if (Core::FileOperations::NovaFileOperations::DirectoryExists(cand)) {
                resolved = Core::FileOperations::NovaFileOperations::NormalizePath(cand);
                break;
            }
        }

        if (resolved.empty()) {
            NOVA_LOG(("ExtensionRegistry: extensions directory '" + extensionsDir + "' not found").c_str(), LogType::Warning);
            return 0;
        }

        auto jsonFiles = Core::FileOperations::NovaFileOperations::FindFiles(resolved, "*.json", true);
        for (const auto& file : jsonFiles) {
            std::string lowerFile = file;
            std::transform(lowerFile.begin(), lowerFile.end(), lowerFile.begin(), ::tolower);
            
            // Skip problematic directories and metadata files
            if (lowerFile.find("intermediate") != std::string::npos ||
                lowerFile.find("menudefinitions") != std::string::npos ||
                lowerFile.find(".schema.json") != std::string::npos ||
                lowerFile.find(".example.json") != std::string::npos) {
                continue;
            }

            // Extension-owned Content is deliberately JSON-based too.  A
            // content action/target must never be mistaken for a loadable
            // extension simply because it has an `id`.  A real descriptor
            // always identifies the module binary through `file`.
            const ExtensionDescriptor candidate = ParseDescriptorFile(fs::path(file));
            if (candidate.id.empty() || candidate.file.empty()) {
                continue;
            }

            if (RegisterDescriptor(file)) ++count;
        }
    } catch (const std::exception& e) {
        NOVA_LOG(("ExtensionRegistry: discovery failed: " + std::string(e.what())).c_str(), LogType::Error);
    }
    return count;
}

bool ExtensionRegistry::RegisterDescriptor(const std::string& descriptorPath) {
    std::string normalizedDescriptor = Core::FileOperations::NovaFileOperations::NormalizePath(descriptorPath);
    if (!Core::FileOperations::NovaFileOperations::FileExists(normalizedDescriptor)) return false;

    fs::path p(normalizedDescriptor);
    ExtensionDescriptor desc = ParseDescriptorFile(p);
    if (desc.id.empty()) {
        NOVA_LOG(("ExtensionRegistry: descriptor " + descriptorPath + " missing 'id'").c_str(), LogType::Error);
        return false;
    }

    // avoid duplicate registration
    if (FindEntry(desc.id)) return false;

    std::string dir = Core::FileOperations::NovaFileOperations::GetParentDirectory(normalizedDescriptor);
    
    // Collect candidate directories: local to the descriptor and the central
    // runtime Binaries folder.  Do not rely solely on the process working
    // directory here: a packaged daemon can be launched through a symlink,
    // systemd, or a distribution launcher whose CWD is not the package root.
    // The descriptor itself is authoritative, so walk upward from it until we
    // locate its enclosing runtime Binaries directory.
    std::vector<std::string> searchDirs;
    searchDirs.push_back(dir);

    std::error_code runtimeRootError;
    auto runtimeCandidate = fs::path(dir);
    while (!runtimeCandidate.empty() && runtimeCandidate != runtimeCandidate.root_path()) {
        const auto binaries = runtimeCandidate / "Binaries";
        runtimeRootError.clear();
        if (fs::is_directory(binaries, runtimeRootError) && !runtimeRootError) {
            searchDirs.push_back(binaries.string());
            break;
        }
        const auto parent = runtimeCandidate.parent_path();
        if (parent == runtimeCandidate) break;
        runtimeCandidate = parent;
    }
    
    // Preserve the relative fallback for source-tree and legacy launches.
    std::string binDir = Core::FileOperations::NovaFileOperations::NormalizePath("Binaries");
    if (Core::FileOperations::NovaFileOperations::DirectoryExists(binDir)) {
        if (std::find(searchDirs.begin(), searchDirs.end(), binDir) == searchDirs.end()) {
            searchDirs.push_back(binDir);
        }
    }

    std::string libpath;
    if (!desc.file.empty()) {
        for (const auto& searchDir : searchDirs) {
            std::string candidate = Core::FileOperations::NovaFileOperations::JoinPaths(searchDir, desc.file);
            if (Core::FileOperations::NovaFileOperations::FileExists(candidate)) {
                libpath = candidate;
                break;
            }
            
            // Fuzzy match in this search directory
            std::string base = Core::FileOperations::NovaFileOperations::GetFileName(desc.file, false);
            std::string base_no_lib = base;
            if (base.rfind("lib", 0) == 0 && base.size() > 3) {
                base_no_lib = base.substr(3);
            }
            
            auto files = Core::FileOperations::NovaFileOperations::FindFiles(searchDir, "*", true);
            for (const auto& fpath : files) {
                if (!SharedLibrary::IsLibraryFile(fpath)) continue;
                auto fname = Core::FileOperations::NovaFileOperations::GetFileName(fpath, true);
                auto stem  = Core::FileOperations::NovaFileOperations::GetFileName(fpath, false);
                bool match = false;
                
                // Direct match or standard variants
                if (fname == desc.file || stem == desc.file || stem == base || stem == base_no_lib ||
                    stem == ("lib" + base_no_lib) || stem == desc.id) {
                    match = true;
                } else if (stem.size() > base_no_lib.size()) {
                    // Config suffixed match (e.g. AegisCore-Development)
                    if (stem.rfind(base_no_lib + "-", 0) == 0)          match = true;
                    if (stem.rfind(base_no_lib + "_", 0) == 0)          match = true;
                    if (stem.rfind(("lib" + base_no_lib) + "-", 0) == 0) match = true;
                    if (stem.rfind(("lib" + base_no_lib) + "_", 0) == 0) match = true;
                }
                
                if (match) {
                    libpath = fpath;
                    break;
                }
            }
            if (!libpath.empty()) break;
        }
    } else {
        for (const auto& searchDir : searchDirs) {
            auto files = Core::FileOperations::NovaFileOperations::FindFiles(searchDir, "*", true);
            for (const auto& fpath : files) {
                if (!SharedLibrary::IsLibraryFile(fpath)) continue;
                auto stem = Core::FileOperations::NovaFileOperations::GetFileName(fpath, false);
                if (stem == desc.id || stem == ("lib" + desc.id) || 
                    stem.rfind(desc.id + "-", 0) == 0 || stem.rfind(desc.id + "_", 0) == 0) {
                    libpath = fpath;
                    break;
                }
            }
            if (!libpath.empty()) break;
        }
    }

    if (libpath.empty() || !Core::FileOperations::NovaFileOperations::FileExists(libpath)) {
        NOVA_LOG(("ExtensionRegistry: cannot locate library for '" + desc.id + "' (descriptor: " + descriptorPath + ")").c_str(), LogType::Error);
        return false;
    }

    auto ent = std::make_unique<Entry>();
    ent->desc = desc;
    ent->descriptorPath = Core::FileOperations::NovaFileOperations::NormalizePath(normalizedDescriptor);
    ent->libFullPath = Core::FileOperations::NovaFileOperations::NormalizePath(libpath);
    ent->state = Entry::Unloaded;
    ent->loadedModule = nullptr;
    entries_.push_back(std::move(ent));
    return true;
}

ExtensionRegistry::Entry* ExtensionRegistry::FindEntry(const std::string& id) {
    for (auto& e : entries_) {
        if (e && e->desc.id == id) return e.get();
    }
    return nullptr;
}

std::vector<ExtensionDescriptor> ExtensionRegistry::ListExtensionDescriptors() const {
    std::vector<ExtensionDescriptor> out;
    for (auto& e : entries_) if (e) out.push_back(e->desc);
    return out;
}

bool ExtensionRegistry::HasExtension(const std::string& id) const {
    for (auto& e : entries_) if (e && e->desc.id == id) return true;
    return false;
}

const ExtensionDescriptor* ExtensionRegistry::GetExtensionDescriptor(const std::string& id) const {
    for (auto& e : entries_) if (e && e->desc.id == id) return &e->desc;
    return nullptr;
}

std::string ExtensionRegistry::GetExtensionDescriptorPath(const std::string& id) const {
    for (const auto& e : entries_) {
        if (e && e->desc.id == id) {
            return e->descriptorPath;
        }
    }
    return "";
}

IExtensionInterface* ExtensionRegistry::GetLoadedExtensionInstance(const std::string& id) const {
    for (const auto& e : entries_) {
        if (!e || e->desc.id != id) {
            continue;
        }
        if (e->state != Entry::Loaded || !e->loadedModule) {
            return nullptr;
        }
        return e->loadedModule->instance;
    }
    return nullptr;
}

void* ExtensionRegistry::GetLoadedExtensionSymbol(const std::string& id, const std::string& symbolName) const {
    if (id.empty() || symbolName.empty()) {
        return nullptr;
    }

    for (const auto& e : entries_) {
        if (!e || e->desc.id != id) {
            continue;
        }

        if (e->state != Entry::Loaded || !e->loadedModule || !e->loadedModule->lib) {
            return nullptr;
        }

        return e->loadedModule->lib->GetSymbol(symbolName);
    }

    return nullptr;
}

bool ExtensionRegistry::IsExtensionLoaded(const std::string& id) const {
    for (const auto& e : entries_) {
        if (e && e->desc.id == id) return e->state == Entry::Loaded;
    }
    return false;
}

std::vector<ExtensionStatusSnapshot> ExtensionRegistry::BuildExtensionStatusSnapshot() const {
    std::vector<ExtensionStatusSnapshot> out;
    out.reserve(entries_.size());

    for (const auto& e : entries_) {
        if (!e) {
            continue;
        }

        ExtensionStatusSnapshot snap;
        snap.id = e->desc.id;
        snap.name = e->desc.name;
        snap.description = e->desc.description;
        snap.version = e->desc.version;
        snap.descriptorPath = e->descriptorPath;
        snap.loaded = e->state == Entry::Loaded;
        snap.exposesCapabilityProvider = false;

        if (snap.loaded && e->loadedModule && e->loadedModule->instance) {
            auto* provider = dynamic_cast<INovaCapabilityProvider*>(e->loadedModule->instance);
            if (provider) {
                snap.exposesCapabilityProvider = true;
                try {
                    const NovaHealthSnapshot health = provider->GetHealthSnapshot();
                    snap.healthStatus = health.status;
                    snap.healthSummary = health.summary;
                    snap.healthUpdatedAtUtc = health.updatedAtUtc;
                } catch (const std::exception& ex) {
                    NOVA_LOG(("ExtensionRegistry: health snapshot query failed for '" + e->desc.id + "': " + ex.what()).c_str(), LogType::Warning);
                } catch (...) {
                    NOVA_LOG(("ExtensionRegistry: health snapshot query failed for '" + e->desc.id + "' due to unknown exception").c_str(), LogType::Warning);
                }
            }
        }

        if (snap.healthStatus.empty()) {
            snap.healthStatus = snap.loaded ? "unknown" : "unloaded";
        }

        if (snap.healthUpdatedAtUtc.empty()) {
            snap.healthUpdatedAtUtc = NowUtcIso8601();
        }

        out.push_back(std::move(snap));
    }

    std::sort(out.begin(), out.end(), [](const ExtensionStatusSnapshot& a, const ExtensionStatusSnapshot& b) {
        if (a.name == b.name) {
            return a.id < b.id;
        }
        return a.name < b.name;
    });

    return out;
}

std::string ExtensionRegistry::BuildExtensionStatusSnapshotJson() const {
    json root;
    root["generatedAtUtc"] = NowUtcIso8601();

    const auto snapshot = BuildExtensionStatusSnapshot();
    json extArray = json::array();

    int loadedCount = 0;
    int providerCount = 0;

    for (const auto& e : snapshot) {
        if (e.loaded) {
            ++loadedCount;
        }
        if (e.exposesCapabilityProvider) {
            ++providerCount;
        }

        json item;
        item["id"] = e.id;
        item["name"] = e.name;
        item["description"] = e.description;
        item["version"] = e.version;
        item["descriptorPath"] = e.descriptorPath;
        item["loaded"] = e.loaded;
        item["exposesCapabilityProvider"] = e.exposesCapabilityProvider;
        item["health"] = {
            {"status", e.healthStatus},
            {"summary", e.healthSummary},
            {"updatedAtUtc", e.healthUpdatedAtUtc}
        };
        extArray.push_back(std::move(item));
    }

    root["summary"] = {
        {"totalExtensions", static_cast<int>(snapshot.size())},
        {"loadedExtensions", loadedCount},
        {"capabilityProviders", providerCount}
    };
    root["extensions"] = std::move(extArray);

    return root.dump(2);
}

bool ExtensionRegistry::LoadExtensionById(const std::string& id) {
    std::unordered_set<std::string> loadingStack;

    std::function<bool(const std::string&)> loadById = [&](const std::string& extensionId) -> bool {
        auto ent = FindEntry(extensionId);
        if (!ent) {
            NOVA_LOG(("ExtensionRegistry: extension '" + extensionId + "' is not registered").c_str(), LogType::Error);
            return false;
        }

        if (ent->state == Entry::Loaded) {
            return true;
        }

        if (loadingStack.find(extensionId) != loadingStack.end()) {
            NOVA_LOG(("ExtensionRegistry: circular dependency detected while loading '" + extensionId + "'").c_str(), LogType::Error);
            ent->state = Entry::Failed;
            return false;
        }

        loadingStack.insert(extensionId);

        for (const auto& dependencyId : ent->desc.dependencies) {
            if (dependencyId.empty()) {
                continue;
            }
            if (dependencyId == extensionId) {
                NOVA_LOG(("ExtensionRegistry: extension '" + extensionId + "' cannot depend on itself").c_str(), LogType::Error);
                ent->state = Entry::Failed;
                loadingStack.erase(extensionId);
                return false;
            }
            if (!HasExtension(dependencyId)) {
                NOVA_LOG(("ExtensionRegistry: missing dependency '" + dependencyId + "' required by '" + extensionId + "'").c_str(), LogType::Error);
                ent->state = Entry::Failed;
                loadingStack.erase(extensionId);
                return false;
            }
            if (!loadById(dependencyId)) {
                NOVA_LOG(("ExtensionRegistry: failed to load dependency '" + dependencyId + "' required by '" + extensionId + "'").c_str(), LogType::Error);
                ent->state = Entry::Failed;
                loadingStack.erase(extensionId);
                return false;
            }
        }

        // Resolve runtime extensionDependencies declared by this descriptor or
        // by a content pack injected via ContentForge. These follow the same
        // topological rule as regular dependencies: ExtC loads before ExtB
        // loads before ExtA. Credentials for backing services (e.g. RabbitMQ)
        // are always sourced via KeyForge after the dependency is loaded.
        for (const auto& depId : ent->desc.extensionDependencies) {
            if (depId.empty() || depId == extensionId) {
                continue;
            }
            if (!HasExtension(depId)) {
                NOVA_LOG(("ExtensionRegistry: missing extensionDependency '" + depId + "' required by content pack in '" + extensionId + "'").c_str(), LogType::Error);
                ent->state = Entry::Failed;
                loadingStack.erase(extensionId);
                return false;
            }
            if (!loadById(depId)) {
                NOVA_LOG(("ExtensionRegistry: failed to load extensionDependency '" + depId + "' required by content pack in '" + extensionId + "'").c_str(), LogType::Error);
                ent->state = Entry::Failed;
                loadingStack.erase(extensionId);
                return false;
            }
        }

        auto lm = loader_.LoadModule(ent->libFullPath);
        if (!lm) {
            ent->state = Entry::Failed;
            loadingStack.erase(extensionId);
            return false;
        }

        ent->loadedModule = lm;
        ent->state = Entry::Loaded;
        loadedOrder_.push_back(extensionId);

        if (ent->loadedModule->instance) {
            try {
                ent->loadedModule->instance->StartupModule();
            } catch (const std::exception& e) {
                NOVA_LOG(("ExtensionRegistry: startup failed for '" + extensionId + "': " + std::string(e.what())).c_str(), LogType::Error);
                loader_.UnloadModule(ent->loadedModule);
                ent->loadedModule = nullptr;
                ent->state = Entry::Failed;
                loadingStack.erase(extensionId);
                return false;
            } catch (...) {
                NOVA_LOG(("ExtensionRegistry: startup failed for '" + extensionId + "' due to unknown exception").c_str(), LogType::Error);
                loader_.UnloadModule(ent->loadedModule);
                ent->loadedModule = nullptr;
                ent->state = Entry::Failed;
                loadingStack.erase(extensionId);
                return false;
            }
        }

        loadingStack.erase(extensionId);
        return true;
    };

    return loadById(id);
}

bool ExtensionRegistry::UnloadExtensionById(const std::string& id) {
    auto ent = FindEntry(id);
    if (!ent) return false;
    for (const auto& candidate : entries_) {
        if (!candidate || candidate->state != Entry::Loaded) continue;
        if (candidate->desc.id == id) continue;
        for (const auto& dependencyId : candidate->desc.dependencies) {
            if (dependencyId == id) {
                NOVA_LOG(("ExtensionRegistry: cannot unload '" + id + "' while dependent extension '" + candidate->desc.id + "' is loaded").c_str(), LogType::Warning);
                return false;
            }
        }
        for (const auto& depId : candidate->desc.extensionDependencies) {
            if (depId == id) {
                NOVA_LOG(("ExtensionRegistry: cannot unload '" + id + "' while content-pack dependency in extension '" + candidate->desc.id + "' is active").c_str(), LogType::Warning);
                return false;
            }
        }
    }
    if (ent->state != Entry::Loaded) return true;
    if (ent->loadedModule && ent->loadedModule->instance) {
        try { ent->loadedModule->instance->ShutdownModule(); } catch (...) {}
    }
    if (ent->loadedModule) {
        loader_.UnloadModule(ent->loadedModule);
        ent->loadedModule = nullptr;
    }
    ent->state = Entry::Unloaded;

    auto it = std::find(loadedOrder_.begin(), loadedOrder_.end(), id);
    if (it != loadedOrder_.end()) {
        loadedOrder_.erase(it);
    }

    return true;
}

void ExtensionRegistry::UnloadAllExtensions() {
    // Unload in reverse order of loading to respect dependencies
    std::vector<std::string> toUnload = loadedOrder_;
    std::reverse(toUnload.begin(), toUnload.end());

    for (const auto& id : toUnload) {
        UnloadExtensionById(id);
    }

    // Fallback for any extensions that might have been loaded outside the normal path
    for (auto& e : entries_) {
        if (e && e->state == Entry::Loaded) {
            UnloadExtensionById(e->desc.id);
        }
    }
}

void ExtensionRegistry::ApplyCliArguments(int argc, const char* argv[]) {
    NOVA_LOG("Dispatching CLI arguments to extensions...", LogType::Log);

    for (const auto& e : entries_) {
        if (!e || e->state != Entry::Loaded || !e->loadedModule || !e->loadedModule->instance) {
            continue;
        }

        auto* cliProvider = dynamic_cast<IExtensionCliProvider*>(e->loadedModule->instance);
        if (cliProvider) {
            auto descriptors = cliProvider->GetCliArgDescriptors();
            if (descriptors.empty()) {
                continue;
            }

            auto matchedArgs = Utils::CommandLineParsing::ParseExtensionArguments(argc, argv, descriptors);
            if (!matchedArgs.empty()) {
                NOVA_LOG(("Applying " + std::to_string(matchedArgs.size()) + " CLI arguments to extension: " + e->desc.id).c_str(), LogType::Log);
                cliProvider->ApplyCliArgs(matchedArgs);
            }
        }
    }
}

ExtensionRegistry::~ExtensionRegistry() {
    UnloadAllExtensions();
}
