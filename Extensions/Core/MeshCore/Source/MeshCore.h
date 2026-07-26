#pragma once

#include "Core/ModuleAPI.h"
#include "Core/IExtensionInterface.h"
#include "Core/FTSTicker.h"
#include "ExtensionSpecific/ICanvasRuntimeSurfaceProvider.h"
#include "ExtensionSpecific/IMenuActionProvider.h"
#include "ExtensionSpecific/INovaCapabilityProvider.h"
#include "MeshCoreClientDelegate.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>

#ifdef MeshCore_EXPORTS
#  define MESHCORE_API NOVA_EXPORT
#else
#  define MESHCORE_API NOVA_IMPORT
#endif

namespace MeshCore {
class MeshClientDelegateImpl;
}

class MESHCORE_API MeshCoreModule : public IExtensionInterface,
                                    public Core::IMenuActionProvider,
                                    public Core::INovaCapabilityProvider {
public:
    MeshCoreModule();
    ~MeshCoreModule() override;

    void StartupModule() override;
    void ShutdownModule() override;

    Core::CanvasMenuActionResult OnMenuAction(const Core::CanvasMenuActionRequest& request) override;

    Core::NovaCapabilityDescriptor GetCapabilityDescriptor() const override;
    Core::NovaHealthSnapshot GetHealthSnapshot() const override;

private:
    MeshCore::FRemoteCommandReceipt SubmitRemoteCommand(const std::string& targetId, const std::string& commandId);
    void UpdateRemoteReceipt(const std::string& receiptId, const MeshCore::FRemoteCommandReceipt& receipt);
    void PublishToast(const std::string& title, const std::string& message, Core::CanvasNotificationSeverity severity) const;

    Core::FTSTicker::FDelegateHandle TickerHandle_;
    std::unique_ptr<MeshCore::MeshClientDelegateImpl> ClientDelegate_;
    mutable std::mutex ReceiptMutex_;
    std::map<std::string, MeshCore::FRemoteCommandReceipt> Receipts_;
};
