# Celestia Nova AI Instructions

## General Rules

- The app is pre-production. Do not introduce deprecation language, deprecation flags, or deprecated compatibility paths.
- When a schema or contract changes, apply the direct model and migrate files accordingly instead of adding deprecated legacy variants.
- Core is only for shared cross-extension code that does not belong to a specific extension.
- If behavior typically belongs to an extension, implement and wire it in that extension, not in Core.
- If extension ownership is blocked, add generic interfaces/helpers in Core so extension code can own the behavior.
- Never wire extension-specific behavior directly in Core.
- Optional descriptor metadata keys must not be hardcoded into Core parsing; extension-specific handling belongs in extensions.

---

## CanvasCore Boundary

CanvasCore renders the skeleton and the base menus (main, options, help, installation). It owns:

- Menu definition loading and rendering (`RunCanvasMenuLoop`).
- Canvas chrome: status pill, toast notifications, persistent info widgets.
- Content injection discovery via `INovaCapabilityProvider` and `IContentInjectionProvider`.
- Cross-instance notification pumping (prefix remote toasts with `[FROM: <instanceId>]`).

CanvasCore must **never** own:

- `deploymentContext`, `apiBasePath`, `targetEnvironment`, `targetFrameworkProvider` — these belong to the extensions (CoreOrchestrator, CoreService, etc.) that actually use them.
- Any orchestration, queue, or credential logic.
- Any per-extension setup or configuration state.

Sub-menus contributed by CoreService, CoreOrchestrator, and specialized extensions are injected into the CanvasCore skeleton through the menu definitions system, not hardcoded into CanvasCore itself.

---

## Extension Interface Rules

- The base extension interface is `IExtensionInterface` (in `Core/IExtensionInterface.h`).
- Extensions implement `StartupModule()` and `ShutdownModule()`.
- The factory boundary is `extern "C" CreateModuleInstance()` / `DestroyModuleInstance()`.
- The `ExtensionRegistry` (singleton) owns extension discovery, loading, and lifecycle.
- `ExtensionDescriptor` holds the descriptor parsed from `.json` files.
- Do not use the old names `IModuleInterface`, `PluginRegistry`, or `PluginDescriptor` — these have been replaced.

---

## FTSTicker – Engine-Loop Ticker

CelestiaNova uses an Unreal Engine–style ticker to drive periodic extension updates without blocking the main menu loop.

- `Core::FTSTicker::GetCoreTicker()` is the singleton core ticker.
- `main()` drives the ticker on a **non-blocking background thread** after startup.
- Extensions subscribe via `FTSTicker::GetCoreTicker().AddTicker(delegate, intervalSeconds)`.
- A delegate returning `false` is automatically unsubscribed.
- `FDelegateHandle` is the subscription token; call `RemoveTicker(handle)` to unsubscribe early.
- Menu ticks (e.g. for spinners) use the same ticker with short intervals.
- Do not block the ticker thread with long-running synchronous work; use async task dispatch instead.

---

## Extension CLI Argument Hand-off

- Extensions that accept CLI arguments implement `IExtensionCliProvider`.
- `GetCliArgDescriptors()` returns the descriptors for arguments the extension can handle.
- After the extension is loaded (autostart or on-demand), CanvasCore (or the host) calls `ApplyCliArgs(args)` with the parsed values matching the extension's descriptors.
- The host `OptionsMenu` / `CommandLineParsing` owns global args. Extension-owned args are separated and forwarded — the extension does its own deeper parsing inside `ApplyCliArgs`.

---

## Cross-Instance Notifications

- Every Celestia Nova instance (host, client, or OS service) can publish notifications through the messaging service.
- Notifications received from **remote** instances (via `IInstanceNotificationBus`, `IsRemote == true`) are prefixed in the toast title: `[FROM: <SourceInstanceId>]`.
- When no menu is currently open, received notifications are logged via `NOVA_LOG` instead of displayed.
- Only authoritative instances (those holding a valid authoritative JWT from AegisCore/NovaID) may push to avoid duplication across the mesh.

---

## AstraLogCore – Distributed Logging Extension

`AstraLogCore` is the logging extension. It is **not** part of Core.

- `NOVA_LOG` macros in Core forward log entries to AstraLogCore when it is loaded.
- AstraLogCore categorizes logs by: sender instance, severity, timestamp, extension id.
- Authoritative instances can push logs to prevent duplication. The authoritative token comes from **NovaID through AegisCore**; it is stored and optionally retrieved via KeyForge.
- AstraLogCore can optionally push logs to a database via a `IDatabaseOrchestratorProvider` when one is configured.
- Logs received from remote instances are tagged with their originating instance id.

---

## MeshCore – Client Mode Delegation

MeshCore in client mode:

1. **Connects** to one authoritative Celestia Nova instance.
2. **Receives** status pill data and connected-instance info from that authoritative instance.
3. **Delegates** work requests through the authoritative instance in a queue (via MessengerOrchestrator).
4. **Polls** responses: the client receives responses through the same authoritative instance, which fetches from the messenger queue.
5. **Reports**: when all queued jobs are finished, a notification slide-in reports `N completed / M failed` via `IInstanceNotificationBus`.

---

## Extension Dependency Resolution Rules

Dependencies between extensions are resolved topologically. The following rules apply:

- An extension (including a content pack registered via NovaAPIServices / ContentForge) may declare `extensionDependencies` in its descriptor. These dependencies are resolved and loaded before the extension activates.
- Example: an `AuthAPI` content pack declares `"extensionDependencies": ["rabbitmq-orchestrator"]`. RabbitMQOrchestrator is resolved, started, and its credentials sourced via KeyForge before the AuthAPI content pack activates.
- Credentials are **always** sourced via KeyForge, regardless of which orchestrator or backend is involved.
- Chained dependencies are resolved fully: if ExtA → ExtB → ExtC, then load order is ExtC, ExtB, ExtA.
- Circular dependencies are rejected at load time by the `ExtensionRegistry`.
- The `RequirementResolver` uses this topological chain when resolving canvas field requirements.
- Content packs injected by ContentForge inherit the dependency rules of the extension that registered them.

---

## OptionsMenu – Mouse Party Mode

- `disableMousePartyMode` is a boolean config entry written to the app Config folder (same mechanism as all other OptionsMenu config entries).
- When enabled, `Event::Mouse` events are suppressed globally in the `CatchEvent` handler of the CanvasCore menu loop (returns `true` without side effects).
- The toggle is exposed in the OptionsMenu UI.
