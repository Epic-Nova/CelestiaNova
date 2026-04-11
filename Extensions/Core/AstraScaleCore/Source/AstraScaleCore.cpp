#include "AstraScaleCore.h"

#include "Core/NovaLog.h"

#include "json.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace {

using json = nlohmann::json;

constexpr const char* kScalingStoreProviderId = "astrascalecore";
constexpr const char* kPersistenceSurfaceId = "astrascalecore.persistence.surface";
constexpr const char* kDefaultPersistenceNamespace = "celestianova.services";
constexpr const char* kDefaultPersistenceCollection = "astra_scale_environments";

struct StoredRecord {
    std::string valueJson;
    std::string updatedAtUtc;
};

struct ManagedEnvironmentStoreData {
    std::vector<Core::SetupSurfaceOption> environments;
    std::string databaseOrchestrator;
    Core::PersistenceBinding binding;
    std::string recordKey;
};

using RecordMap = std::unordered_map<std::string, StoredRecord>;

std::unordered_map<std::string, RecordMap> gDbStore;
std::mutex gDbStoreMutex;

static std::string BuildUtcTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto nowTimeT = std::chrono::system_clock::to_time_t(now);
    std::tm utcTime {};
#if defined(_WIN32)
    gmtime_s(&utcTime, &nowTimeT);
#else
    gmtime_r(&nowTimeT, &utcTime);
#endif
    char buffer[32] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utcTime);
    return buffer;
}

static std::string BuildBucketKey(const Core::PersistenceBinding& binding) {
    return binding.providerId + "|" + binding.orchestratorId + "|" + binding.databaseName + "|" + binding.collectionName;
}

static bool ContainsEnvironmentOption(const std::vector<Core::SetupSurfaceOption>& options,
                                      const std::string& value) {
    return std::any_of(options.begin(), options.end(), [&](const Core::SetupSurfaceOption& option) {
        return option.value == value;
    });
}

static std::vector<Core::SetupSurfaceOption> ParseManagedEnvironments(const json& payload) {
    std::vector<Core::SetupSurfaceOption> options;
    if (!payload.contains("environments") || !payload["environments"].is_array()) {
        return options;
    }

    for (const auto& environment : payload["environments"]) {
        if (environment.is_string()) {
            const std::string value = environment.get<std::string>();
            if (value.empty()) {
                continue;
            }
            options.push_back({
                value,
                value,
                "Configured in AstraScaleCore persistence store",
                false,
            });
            continue;
        }

        if (!environment.is_object()) {
            continue;
        }

        Core::SetupSurfaceOption option;
        option.value = environment.value("value", environment.value("id", ""));
        option.label = environment.value("label", option.value);
        option.description = environment.value("description", "Configured in AstraScaleCore persistence store");
        option.recommended = environment.value("recommended", false);

        if (!option.value.empty()) {
            options.push_back(std::move(option));
        }
    }

    return options;
}

static std::vector<Core::SetupSurfaceOption> BuildDefaultManagedEnvironments(const Core::SetupSurfaceRequest& request) {
    if (!request.managedEnvironmentOptions.empty()) {
        return request.managedEnvironmentOptions;
    }

    if (!request.environment.empty()) {
        return {
            {
                request.environment,
                request.environment,
                "Derived from active setup request context",
                true,
            },
        };
    }

    return {
        {
            "default",
            "Default",
            "Configure managed environments in AstraScaleCore persistence store",
            true,
        },
    };
}

static Core::PersistenceBinding BuildPersistenceBinding(const Core::SetupSurfaceRequest& request) {
    Core::PersistenceBinding binding;
    binding.providerId = request.persistenceSurfaceId.empty() ? kPersistenceSurfaceId : request.persistenceSurfaceId;
    binding.orchestratorId = request.scalingStoreRef;
    if (binding.orchestratorId.empty() && !request.activeDatabaseOrchestrators.empty()) {
        binding.orchestratorId = request.activeDatabaseOrchestrators.front();
    }
    binding.databaseName = request.persistenceNamespace.empty() ? kDefaultPersistenceNamespace : request.persistenceNamespace;
    binding.collectionName = request.persistenceCollection.empty() ? kDefaultPersistenceCollection : request.persistenceCollection;
    binding.scope = Core::PersistenceOperationScope::Service;
    binding.required = false;
    return binding;
}

static std::string BuildPersistenceRecordKey(const Core::SetupSurfaceRequest& request) {
    if (!request.serviceId.empty()) {
        return "service:" + request.serviceId + ":managed-environments";
    }
    if (!request.extensionId.empty()) {
        return "extension:" + request.extensionId + ":managed-environments";
    }
    return "global:managed-environments";
}

static bool ReadScaffoldRecord(const Core::PersistenceReadRequest& request, Core::PersistenceRecord& outRecord) {
    const std::string bucketKey = BuildBucketKey(request.binding);
    std::scoped_lock lock(gDbStoreMutex);
    const auto bucketIt = gDbStore.find(bucketKey);
    if (bucketIt == gDbStore.end()) {
        return false;
    }

    const auto recordIt = bucketIt->second.find(request.key);
    if (recordIt == bucketIt->second.end()) {
        return false;
    }

    outRecord.key = request.key;
    outRecord.valueJson = recordIt->second.valueJson;
    outRecord.updatedAtUtc = recordIt->second.updatedAtUtc;
    return true;
}

static bool WriteScaffoldRecord(const Core::PersistenceWriteRequest& request) {
    if (request.key.empty()) {
        return false;
    }

    const std::string bucketKey = BuildBucketKey(request.binding);
    std::scoped_lock lock(gDbStoreMutex);
    RecordMap& bucket = gDbStore[bucketKey];

    if (!request.upsert && bucket.find(request.key) != bucket.end()) {
        return false;
    }

    bucket[request.key] = {
        request.valueJson,
        BuildUtcTimestamp(),
    };
    return true;
}

static std::vector<Core::PersistenceRecord> ListScaffoldRecords(const Core::PersistenceListRequest& request) {
    const std::string bucketKey = BuildBucketKey(request.binding);
    std::vector<Core::PersistenceRecord> records;

    std::scoped_lock lock(gDbStoreMutex);
    const auto bucketIt = gDbStore.find(bucketKey);
    if (bucketIt == gDbStore.end()) {
        return records;
    }

    for (const auto& [recordKey, recordValue] : bucketIt->second) {
        if (!request.keyPrefix.empty() && recordKey.rfind(request.keyPrefix, 0) != 0) {
            continue;
        }

        records.push_back({
            recordKey,
            recordValue.valueJson,
            recordValue.updatedAtUtc,
        });
    }

    return records;
}

static json BuildManagedEnvironmentPayload(const std::vector<Core::SetupSurfaceOption>& environments,
                                           const std::string& databaseOrchestrator,
                                           const std::string& scalingManagerId,
                                           const Core::PersistenceBinding& binding,
                                           const std::string& recordKey) {
    json payload;
    payload["scalingManagerId"] = scalingManagerId.empty() ? kScalingStoreProviderId : scalingManagerId;
    payload["databaseOrchestrator"] = databaseOrchestrator;
    payload["binding"] = {
        {"providerId", binding.providerId},
        {"orchestratorId", binding.orchestratorId},
        {"databaseName", binding.databaseName},
        {"collectionName", binding.collectionName},
    };
    payload["recordKey"] = recordKey;
    payload["environments"] = json::array();

    for (const auto& environment : environments) {
        payload["environments"].push_back({
            {"id", environment.value},
            {"value", environment.value},
            {"label", environment.label.empty() ? environment.value : environment.label},
            {"description", environment.description},
            {"recommended", environment.recommended},
        });
    }

    return payload;
}

static ManagedEnvironmentStoreData LoadManagedEnvironmentStore(const Core::SetupSurfaceRequest& request) {
    ManagedEnvironmentStoreData data;
    data.binding = BuildPersistenceBinding(request);
    data.recordKey = BuildPersistenceRecordKey(request);

    bool writeBackStore = false;
    Core::PersistenceRecord persisted;
    if (ReadScaffoldRecord({data.binding, data.recordKey}, persisted) && !persisted.valueJson.empty()) {
        try {
            const json payload = json::parse(persisted.valueJson);
            data.environments = ParseManagedEnvironments(payload);
            if (payload.contains("databaseOrchestrator") && payload["databaseOrchestrator"].is_string()) {
                data.databaseOrchestrator = payload["databaseOrchestrator"].get<std::string>();
            }
        } catch (const std::exception& ex) {
            NOVA_LOG((std::string("[AstraScaleCore] failed parsing managed environment store payload: ") + ex.what()).c_str(), LogType::Warning);
            writeBackStore = true;
        }
    } else {
        writeBackStore = true;
    }

    if (data.environments.empty()) {
        data.environments = BuildDefaultManagedEnvironments(request);
        writeBackStore = true;
    }

    if (!request.environment.empty() && !ContainsEnvironmentOption(data.environments, request.environment)) {
        data.environments.push_back({
            request.environment,
            request.environment,
            "Derived from active setup request context",
            false,
        });
        writeBackStore = true;
    }

    if (data.databaseOrchestrator.empty()) {
        data.databaseOrchestrator = data.binding.orchestratorId;
        if (!data.databaseOrchestrator.empty()) {
            writeBackStore = true;
        }
    }

    if (writeBackStore) {
        const json payload = BuildManagedEnvironmentPayload(
            data.environments,
            data.databaseOrchestrator,
            request.scalingManagerExtensionId,
            data.binding,
            data.recordKey);

        if (!WriteScaffoldRecord({data.binding, data.recordKey, payload.dump(2), true})) {
            NOVA_LOG("[AstraScaleCore] failed writing managed environment store payload", LogType::Warning);
        }
    }

    return data;
}

} // namespace

AstraScaleCoreModule::AstraScaleCoreModule() {}
AstraScaleCoreModule::~AstraScaleCoreModule() {}

void AstraScaleCoreModule::StartupModule() {
    NOVA_LOG("[AstraScaleCore] StartupModule called", LogType::Log);
}

void AstraScaleCoreModule::ShutdownModule() {
    NOVA_LOG("[AstraScaleCore] ShutdownModule called", LogType::Log);
}

Core::NovaCapabilityDescriptor AstraScaleCoreModule::GetCapabilityDescriptor() const {
    Core::NovaCapabilityDescriptor descriptor;
    descriptor.providerId = "astrascalecore";
    descriptor.displayName = "AstraScaleCore";
    descriptor.description = "Environment-aware scaling core with DB-orchestrator-backed persistence scaffolding.";
    descriptor.serviceCapabilities = {
        "scaling.policy.apply",
        "scaling.recommendations.list",
        "environments.manage",
        "environments.promote",
        "environments.rollback",
        "setup.surface.inject",
        "secure.env.inject",
        "lifecycle.gate.validate",
        "persistence.store.read",
        "persistence.store.write",
        "persistence.surface.provide",
    };
    descriptor.healthEndpoints = {"/api/v1/health/astrascalecore"};
    descriptor.contentPacks = {"ScaleControlPanelPack", "EnvironmentPolicyPack"};
    descriptor.contentEndpoints = {
        "/api/v1/content/astrascalecore",
        "/api/v1/content/astrascalecore/policies",
        "/api/v1/content/astrascalecore/environments",
        "/api/v1/content/astrascalecore/persistence",
    };
    descriptor.telemetryStreams = {"astrascale.scale.actions", "astrascale.policy.evals"};
    descriptor.grafanaDashboards = {"grafana://celestianova/astrascale-core"};
    return descriptor;
}

Core::NovaHealthSnapshot AstraScaleCoreModule::GetHealthSnapshot() const {
    Core::NovaHealthSnapshot health;
    health.status = "healthy";
    health.summary = "Scaling policy engine and persistence surface initialized";
    return health;
}

std::vector<Core::SetupProfileDepth> AstraScaleCoreModule::GetSupportedProfileDepths() const {
    return {
        Core::SetupProfileDepth::Auto,
        Core::SetupProfileDepth::Simple,
        Core::SetupProfileDepth::Advanced,
        Core::SetupProfileDepth::Expert,
    };
}

Core::OrchestratorSetupSurface AstraScaleCoreModule::BuildSetupSurface(
    const Core::SetupSurfaceRequest& request,
    Core::SetupProfileDepth depth) const {
    const ManagedEnvironmentStoreData managedStore = LoadManagedEnvironmentStore(request);

    Core::OrchestratorSetupSurface surface;
    surface.depth = depth;
    surface.uiContributors = request.activeOrchestrators;
    surface.uiContributors.push_back(kScalingStoreProviderId);

    Core::SetupSurfaceSection scaleSection;
    scaleSection.id = "scaling-control";
    scaleSection.title = "Scaling Control";
    scaleSection.description = "Inject scaling controls directly into service setup workflows.";
    scaleSection.fields.push_back({
        "enableScaling",
        "Enable autoscaling",
        Core::SetupFieldType::Toggle,
        false,
        "",
        request.scalingRequested ? "true" : "false",
        {}
    });
    scaleSection.fields.push_back({
        "minReplicas",
        "Minimum replicas",
        Core::SetupFieldType::Number,
        true,
        "1",
        "1",
        {}
    });
    scaleSection.fields.push_back({
        "maxReplicas",
        "Maximum replicas",
        Core::SetupFieldType::Number,
        true,
        "5",
        "3",
        {}
    });

    Core::SetupSurfaceSection envSection;
    envSection.id = "environment-routing";
    envSection.title = "Environment Routing";
    envSection.description = "Map deployment environments from AstraScaleCore DB-orchestrator-backed persistence store.";

    Core::SetupSurfaceField envField;
    envField.key = "targetEnvironment";
    envField.label = "Target environment";
    envField.type = Core::SetupFieldType::Select;
    envField.required = true;
    envField.placeholder = "Configured by AstraScaleCore managed environment persistence store";
    envField.defaultValue = request.environment.empty()
        ? (managedStore.environments.empty() ? "" : managedStore.environments.front().value)
        : request.environment;
    envField.options = managedStore.environments;
    envSection.fields.push_back(std::move(envField));

    Core::SetupSurfaceField dbField;
    dbField.key = "databaseProvider";
    dbField.label = "Database provider";
    dbField.type = Core::SetupFieldType::Select;
    dbField.required = true;
    for (const auto& dbOrchestrator : request.activeDatabaseOrchestrators) {
        dbField.options.push_back({dbOrchestrator, dbOrchestrator, "Available through active orchestrator", false});
    }
    if (dbField.options.empty() && !managedStore.databaseOrchestrator.empty()) {
        dbField.options.push_back({
            managedStore.databaseOrchestrator,
            managedStore.databaseOrchestrator,
            "Persisted in AstraScaleCore managed environment store",
            true
        });
    }
    if (dbField.options.empty()) {
        dbField.required = false;
        dbField.placeholder = "No active database orchestrator available in setup context";
    } else {
        dbField.defaultValue = managedStore.databaseOrchestrator;
    }
    envSection.fields.push_back(std::move(dbField));

    Core::SetupSurfaceSection persistenceSection;
    persistenceSection.id = "persistence-surface";
    persistenceSection.title = "Persistence Surface";
    persistenceSection.description = "DB-backed persistence binding for service and orchestrator state.";
    persistenceSection.fields.push_back({
        "persistenceSurfaceId",
        "Persistence surface ID",
        Core::SetupFieldType::InputText,
        true,
        "e.g. astrascalecore.persistence.surface",
        managedStore.binding.providerId,
        {}
    });
    persistenceSection.fields.push_back({
        "persistenceNamespace",
        "Persistence namespace",
        Core::SetupFieldType::InputText,
        true,
        "e.g. celestianova.services",
        managedStore.binding.databaseName,
        {}
    });
    persistenceSection.fields.push_back({
        "persistenceCollection",
        "Persistence collection",
        Core::SetupFieldType::InputText,
        true,
        "e.g. astra_scale_environments",
        managedStore.binding.collectionName,
        {}
    });

    surface.sections.push_back(std::move(scaleSection));
    surface.sections.push_back(std::move(envSection));
    surface.sections.push_back(std::move(persistenceSection));
    return surface;
}

Core::InteractionLifecycleContract AstraScaleCoreModule::GetInteractionLifecycleContract() const {
    Core::InteractionLifecycleContract contract;
    contract.providerId = "astrascalecore";
    contract.summary = "Environment and scaling lifecycle contract for service setup and runtime operation.";
    contract.actions = {
        {Core::InteractionLifecycleStage::Configuration, Core::InteractionActionType::Validate, "scale.config.validate", "Validate scaling config", "Validate scale thresholds and environment policy", false, false},
        {Core::InteractionLifecycleStage::Configuration, Core::InteractionActionType::Validate, "scale.gate.validate", "Validate promotion gates", "Validate lifecycle gates before allowing environment promotion", false, true},
        {Core::InteractionLifecycleStage::Configuration, Core::InteractionActionType::Apply, "scale.config.apply", "Apply scaling config", "Apply scaling policy and secure environment mappings", true, true},
        {Core::InteractionLifecycleStage::Migration, Core::InteractionActionType::Preview, "env.migration.preview", "Preview environment migration", "Preview environment rule migration before apply", false, false},
        {Core::InteractionLifecycleStage::Migration, Core::InteractionActionType::Apply, "env.migration.apply", "Run environment migration", "Apply environment mapping and policy migration", true, true},
        {Core::InteractionLifecycleStage::Migration, Core::InteractionActionType::Apply, "env.promote", "Promote environment", "Promote validated configuration from staging policy to live policy", true, true},
        {Core::InteractionLifecycleStage::Interaction, Core::InteractionActionType::Apply, "scale.recommend", "Apply scale recommendation", "Apply recommendation from health and load metrics", true, true},
        {Core::InteractionLifecycleStage::Interaction, Core::InteractionActionType::Rollback, "env.rollback", "Rollback environment", "Rollback environment policy and scaling profile to the last known good state", true, true},
        {Core::InteractionLifecycleStage::Interaction, Core::InteractionActionType::FetchLogs, "scale.logs", "Fetch scaling logs", "Fetch scaling decision logs", false, false},
    };
    contract.emittedEvents = {
        "astrascale.config.applied",
        "astrascale.environment.updated",
        "astrascale.environment.promoted",
        "astrascale.environment.rolledback",
        "astrascale.policy.executed"
    };
    return contract;
}

std::string AstraScaleCoreModule::GetPersistenceSurfaceId() const {
    return kPersistenceSurfaceId;
}

bool AstraScaleCoreModule::ReadRecord(const Core::PersistenceReadRequest& request,
                                      Core::PersistenceRecord& outRecord) const {
    return ReadScaffoldRecord(request, outRecord);
}

bool AstraScaleCoreModule::WriteRecord(const Core::PersistenceWriteRequest& request) const {
    return WriteScaffoldRecord(request);
}

std::vector<Core::PersistenceRecord> AstraScaleCoreModule::ListRecords(const Core::PersistenceListRequest& request) const {
    return ListScaffoldRecords(request);
}

std::vector<Core::PersistenceBinding> AstraScaleCoreModule::GetPersistenceBindings() const {
    return {
        {
            kPersistenceSurfaceId,
            "",
            kDefaultPersistenceNamespace,
            kDefaultPersistenceCollection,
            Core::PersistenceOperationScope::Service,
            false,
        },
    };
}
