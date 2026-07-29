# Linux service mode

Celestia Nova can run without CanvasCore's interactive menu as a supervised Linux service. It loads the normal extension set, runs the shared ticker, and writes an atomic status snapshot for the status API and dashboard layer.

The service mode is intentionally **not** an HTTP server. NovaAPIService or a future authenticated service gateway must expose the snapshot. This avoids accidentally publishing an unauthenticated control plane from a host daemon.

## Runtime contract

The declarative contract is [LinuxServiceMode.json](../Content/ServiceMode/LinuxServiceMode.json). The process accepts:

- `--service-mode` (or the compatibility alias `--daemon`): start headless service mode.
- `--status-file <path>`: output JSON path; default `Runtime/status/service-status.json`.
- `--status-interval-seconds <5-3600>`: heartbeat interval; default `15`.

Snapshots are written to a temporary sibling and renamed atomically. They contain only the existing `StatusApiSurface` extension lifecycle/health payload; KeyForge material, access tokens, and runtime secrets are excluded.

## Docker bootstrap boundary

The installer deploys a root-owned, argumentless helper at
`/usr/local/lib/celestianova/bootstrap-docker` and grants the `celestianova`
service user passwordless sudo for that exact path only. DockerOrchestrator may
use it to install `docker.io` and `docker-compose-v2`, enable Docker, and add
the service account to the Docker group. It cannot execute arbitrary package
or shell commands. Restart `celestianova.service` after a successful bootstrap
before running Compose actions.

## Install on a VM

Build a Linux package, then install it with the package-owned utility:

```bash
sudo ./Utilities/linux/install_service_mode.sh \
  ./Artifacts/CelestiaNova-Linux-Production
```

The installer verifies the package layout, creates the dedicated non-login
account, installs under `/opt/celestianova`, grants write access only to the
runtime log and status directories, and enables the systemd unit.

Operational lifecycle:

```bash
sudo systemctl status celestianova
sudo systemctl restart celestianova
journalctl -u celestianova -f
cat /var/lib/celestianova/status/service-status.json
```

`systemd` sends `SIGTERM` on stop; Celestia Nova emits one final status snapshot and exits cleanly. The unit uses a dedicated non-login account and only permits writes to `/var/lib/celestianova`.

## Explicit local Auth API test routing

Production Auth API calls remain HTTPS-only. A VM test node can explicitly
route its own daemon and `celest` commands to the co-located Auth API:

```bash
sudo /usr/local/lib/celestianova/configure-local-auth-api-test
```

The command writes a root-owned, credential-free `/etc/celestianova/runtime.env`
with `CELESTIA_AUTH_API_BASE_URL=http://127.0.0.1:8081` and
`CELESTIA_LOCAL_TEST_MODE=1`. Plain HTTP is accepted only for that exact base
and only below `/api/v1/`; it is never a production fallback. A Windows client
must configure its own host-only VM URL with
`Utilities/windows/configure_local_auth_api_test.ps1`.

For a device-login link that opens from the Windows host, supply the VM's
host-only address as the optional second argument. It becomes the Auth API
container's `APP_URL`, while daemon-to-Auth calls still use loopback:

```bash
sudo /usr/local/lib/celestianova/configure-local-auth-api-test \
  http://127.0.0.1:8081 http://192.168.50.162:8081
```

## Status API/dashboard integration

NexusCore owns normalized daemon-status aggregation: extension lifecycle and
health, progress, declared capabilities, Mesh connectivity, and a bounded
SignalCore event feed. PulseCore remains the telemetry owner and SignalCore
the notification/event owner. NovaAPIService only hosts this data through the
daemon's read-only local HTTP surface on the loopback interface (default port
`9080`; configure
`CELESTIA_STATUS_PORT` through a systemd drop-in):

```bash
curl http://127.0.0.1:9080/api/v1/health
curl http://127.0.0.1:9080/api/v1/status
curl http://127.0.0.1:9080/api/v1/progress
```

This is intentionally separate from an application health endpoint such as
Auth API. It must remain loopback-only until an authenticated gateway proxies
it for a remote dashboard.

SyncForge is the sole owner of Celestia Nova update checks. It requests the
Auth API update manifest through KeyForge's protected OAuth broker and exposes
the redacted check state through the same status surface. Package application
remains fail-closed until the root-owned, signature-verifying SyncForge updater
is installed.

The consumer reads `/var/lib/celestianova/status/service-status.json` through the already-authenticated Nova API/status surface. It must report the file modification time as the daemon heartbeat and mark it stale after at least twice `statusIntervalSeconds`. It must not use a missing file as proof that a managed application is healthy.

Managed application health belongs in the owning orchestrator's extension health snapshot; the daemon aggregates it, rather than probing or starting arbitrary applications itself.
# `celest` command surface

The installer registers `/usr/local/bin/celest`. It uses the same extension
commands as the graphical surface:

```bash
celest help
celest status
celest progress
celest complete de
celest deploy auth-api minimal
celest stop auth-api
celest run --docker-bootstrap
```

`celest progress` renders the latest atomic progress snapshot. Extensions
publish this shared snapshot, which is also suitable for Canvas and MeshCore.
