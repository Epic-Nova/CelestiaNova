#include "AegisCore.h"

#include "Core/NovaLog.h"

AegisCoreModule::AegisCoreModule() {}
AegisCoreModule::~AegisCoreModule() {}

void AegisCoreModule::StartupModule() {
    NOVA_LOG("[AegisCore] StartupModule called", LogType::Log);
}

void AegisCoreModule::ShutdownModule() {
    NOVA_LOG("[AegisCore] ShutdownModule called", LogType::Log);
}
