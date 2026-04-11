#include "SyncForge.h"

#include "Core/NovaLog.h"

SyncForgeModule::SyncForgeModule() {}
SyncForgeModule::~SyncForgeModule() {}

void SyncForgeModule::StartupModule() {
    NOVA_LOG("[SyncForge] StartupModule called", LogType::Log);
}

void SyncForgeModule::ShutdownModule() {
    NOVA_LOG("[SyncForge] ShutdownModule called", LogType::Log);
}
