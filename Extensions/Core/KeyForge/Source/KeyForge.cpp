#include "KeyForge.h"

#include "Core/NovaLog.h"

#include <sstream>

KeyForgeModule::KeyForgeModule() {}
KeyForgeModule::~KeyForgeModule() {}

void KeyForgeModule::StartupModule() {
    NOVA_LOG("[KeyForge] StartupModule called", LogType::Log);
}

void KeyForgeModule::ShutdownModule() {
    NOVA_LOG("[KeyForge] ShutdownModule called", LogType::Log);
}

bool KeyForgeModule::AcceptEnvironmentTargetHandoff(const std::string& requestorExtensionId,
                                                    const std::string& environmentTarget,
                                                    std::string& outReceipt) {
    if (environmentTarget.empty()) {
        outReceipt = "rejected: empty environment target";
        NOVA_LOG("[KeyForge] Example handoff rejected (empty environment target)", LogType::Warning);
        return false;
    }

    std::ostringstream message;
    message << "[KeyForge] Example handoff accepted from '" << requestorExtensionId
            << "' for environment target '" << environmentTarget << "'";
    NOVA_LOG(message.str().c_str(), LogType::Log);

    outReceipt = "accepted: " + requestorExtensionId + " -> " + environmentTarget;
    return true;
}
