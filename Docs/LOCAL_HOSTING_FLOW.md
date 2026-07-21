# Local Hosting Flow

## Current Auth API deployment

1. ContentForge reads `Content/ContentPacks/AuthApiLocal.json` and registers the local `auth-api` content.
2. LaravelOrchestrator exposes the content in its Canvas menu.
3. Start and stop actions are delegated to DockerOrchestrator.
4. DockerOrchestrator uses TerminalAgent to run the matching Docker Compose command asynchronously.
5. Canvas remains responsive and receives immediate acknowledgement plus completion toasts.
6. The local Auth API health endpoint is available at `http://127.0.0.1/api/health`.

## Roles

| Component | Responsibility |
| --- | --- |
| ContentForge | Owns content metadata and resolves local content paths. |
| LaravelOrchestrator | Validates Laravel content and exposes lifecycle controls. |
| DockerOrchestrator | Runs controlled Compose lifecycle actions. |
| TerminalAgent | Executes lifecycle commands asynchronously and reports completion. |
| CanvasCore | Provides menus, status feedback, and deployment toasts. |

## Next step

Replace the fixed `auth-api` menu entry with a ContentForge-driven list of registered Laravel content packs, so each compatible application receives its own lifecycle controls without code changes.
