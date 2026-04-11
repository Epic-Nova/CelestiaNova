#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"

#ifdef RabbitMQOrchestrator_EXPORTS
#  define RABBITMQORCHESTRATOR_API NOVA_EXPORT
#else
#  define RABBITMQORCHESTRATOR_API NOVA_IMPORT
#endif

class RABBITMQORCHESTRATOR_API RabbitMQOrchestratorModule : public IExtensionInterface {
public:
    RabbitMQOrchestratorModule();
    ~RabbitMQOrchestratorModule() override;

    void StartupModule() override;
    void ShutdownModule() override;
};

#ifdef RabbitMQOrchestrator_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(RABBITMQORCHESTRATOR_API, RabbitMQOrchestratorModule)
#endif

