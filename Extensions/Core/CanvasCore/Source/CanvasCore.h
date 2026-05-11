#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "CanvasMenuService.h"
#include "ExtensionSpecific/ICanvasRuntimeSurfaceProvider.h"

namespace CanvasCore {
class CanvasMenuRuntime;
}

#ifdef CanvasCore_EXPORTS
#  define CANVASCORE_API NOVA_EXPORT
#else
#  define CANVASCORE_API NOVA_IMPORT
#endif

class CANVASCORE_API CanvasCoreModule : public IExtensionInterface,
                                        public CanvasCore::ICanvasMenuService,
                                        public Core::ICanvasRuntimeSurfaceProvider {
public:
    CanvasCoreModule();
    ~CanvasCoreModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    bool ReloadMenuDefinitions(std::vector<Core::FJsonParseIssue>& outIssues) override;
    std::vector<std::string> ListMenuIds() const override;
    bool GetMenuDefinition(const std::string& menuId,
                           CanvasCore::MenuSchema::FCanvasMenuDefinition& outMenu) const override;

    CanvasCore::MenuSchema::FCanvasRequirementResolveResult ResolveFieldRequirement(
        const std::string& menuId,
        const std::string& fieldId,
        const std::string& consumerExtensionId,
        const std::vector<CanvasCore::FCanvasFieldValue>& contextValues) const override;

    bool BuildSubmitPayload(const std::string& menuId,
                            const std::vector<CanvasCore::FCanvasFieldValue>& collectedValues,
                            std::string& outPayloadJson,
                            std::string& outError) const override;

    bool BuildMenuRenderFrame(const std::string& menuId,
                              std::size_t maxToasts,
                              CanvasCore::FCanvasMenuRenderFrame& outFrame,
                              std::string& outError) const override;

    bool RunCanvasMenuLoop(const std::string& startMenuId,
                           std::string& outError) override;

    Core::CanvasStatusPillSnapshot GetCanvasStatusPill() const override;

    std::vector<Core::CanvasToastNotification> ConsumeCanvasToastsForMenu(
        const std::string& menuId,
        std::size_t maxCount) const override;

    std::vector<Core::CanvasToastNotification> ConsumeCanvasToasts(std::size_t maxCount) const override;

    std::vector<Core::CanvasPersistentInfoWidget> GetCanvasPersistentInfos(const std::string& menuId) const override;

    void PublishCanvasToast(const Core::CanvasToastNotification& toast) override;

private:
    void PumpSignalNotifications() const;
    void QueueToastLocked(Core::CanvasToastNotification toast) const;
    void QueueToast(Core::CanvasToastNotification toast) const;

    void RecordPersistentInfo(const std::string& menuId,
                              const std::string& fieldId,
                              const std::string& code,
                              const std::string& message,
                              const std::string& source) const;

    void ClearPersistentInfosForField(const std::string& menuId, const std::string& fieldId) const;
    void ClearPersistentInfosForMenu(const std::string& menuId) const;

    std::unique_ptr<CanvasCore::CanvasMenuRuntime> Runtime_;

    mutable std::mutex UiStateMutex_;
    mutable std::deque<Core::CanvasToastNotification> ToastQueue_;
    mutable std::unordered_map<std::string, std::vector<Core::CanvasPersistentInfoWidget>> PersistentInfosByMenu_;
    mutable std::unordered_set<std::string> PersistentInfoKeys_;
    mutable std::uint64_t LastObservedSignalSequence_ = 0;
    mutable std::uint64_t RuntimeNotificationCounter_ = 0;
    mutable bool HasReloadedOnFirstUse_ = false;
    mutable std::vector<std::string> MenuHistory_;
    mutable std::function<void()> RedrawCallback_;
};

#ifdef CanvasCore_EXPORTS
NOVA_DECLARE_MODULE_FACTORY(CANVASCORE_API, CanvasCoreModule)
#endif

