# Local Hosting Flow

## Current Auth API deployment

1. ContentForge discovers all root and extension-owned content-pack manifests, including `auth-api`.
2. LaravelOrchestrator verifies that the selected pack belongs to Laravel and materializes an immutable local release under `Content/.runtime/<content-id>/<release-id>`.
3. DockerOrchestrator runs `docker compose config -q` against that release before accepting any lifecycle job.
4. Start, stop, status, and logs use tracked asynchronous Compose jobs (`accepted`, `running`, `succeeded`, `failed`) through TerminalAgent.
5. Canvas remains responsive and receives immediate acknowledgement with the Docker job ID.
6. The local Auth API health endpoint is available at `http://127.0.0.1/api/health` once Docker is running and the runtime environment has been injected.

## Roles

| Component | Responsibility |
| --- | --- |
| ContentForge | Owns content metadata and materializes non-secret local releases. |
| LaravelOrchestrator | Validates Laravel content, tracks selected releases, and exposes lifecycle controls. |
| DockerOrchestrator | Validates Compose and runs tracked lifecycle jobs. |
| TerminalAgent | Executes lifecycle commands asynchronously and reports completion. |
| CanvasCore | Provides menus, status feedback, and deployment toasts. |

## Next step

Add a ContentForge requirement resolver so the generic `contentId` input becomes a dropdown of compatible Laravel packs. Then add KeyForge-backed local environment injection; ContentForge deliberately does not copy `.env` files or private keys into release folders.
