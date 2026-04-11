#pragma once

#include <string>
#include <vector>

#include "Core/ModuleAPI.h"

namespace Core {

enum class PersistenceOperationScope {
    Extension,
    Service,
    Shared,
};

struct PersistenceBinding {
    std::string providerId;
    std::string orchestratorId;
    std::string databaseName;
    std::string collectionName;
    PersistenceOperationScope scope = PersistenceOperationScope::Service;
    bool required = false;
};

struct PersistenceRecord {
    std::string key;
    std::string valueJson;
    std::string updatedAtUtc;
};

struct PersistenceReadRequest {
    PersistenceBinding binding;
    std::string key;
};

struct PersistenceWriteRequest {
    PersistenceBinding binding;
    std::string key;
    std::string valueJson;
    bool upsert = true;
};

struct PersistenceListRequest {
    PersistenceBinding binding;
    std::string keyPrefix;
};

// Implemented by extensions that expose a persistence surface used by
// orchestrators and services for structured state storage.
class NOVA_CORE_API INovaPersistenceSurface {
public:
    virtual ~INovaPersistenceSurface();

    virtual std::string GetPersistenceSurfaceId() const = 0;
    virtual bool ReadRecord(const PersistenceReadRequest& request, PersistenceRecord& outRecord) const = 0;
    virtual bool WriteRecord(const PersistenceWriteRequest& request) const = 0;
    virtual std::vector<PersistenceRecord> ListRecords(const PersistenceListRequest& request) const = 0;
};

// Optional metadata provider for persistence bindings contributed by an extension.
class NOVA_CORE_API INovaPersistenceSurfaceProvider {
public:
    virtual ~INovaPersistenceSurfaceProvider();

    virtual std::vector<PersistenceBinding> GetPersistenceBindings() const = 0;
};

} // namespace Core