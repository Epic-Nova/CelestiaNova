#include "Core/NovaCore.h"
#include "Core/ModuleManager.h"
#include "Core/NovaLog.h"

namespace Core {

void InitializeExtensions(const std::string& extensionsDir) {
    NOVA_LOG((std::string("Initializing extensions from: ") + extensionsDir).c_str(), LogType::Log);
    int n = ModuleManager::Instance().DiscoverAndLoad(extensionsDir);
    NOVA_LOG((std::string("Loaded ") + std::to_string(n) + " extensions.").c_str(), LogType::Log);
}

} // namespace Core
