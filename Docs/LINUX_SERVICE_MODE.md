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

## Status API/dashboard integration

NovaAPIService exposes the daemon's read-only local status surface on the
loopback interface only (default port `9080`; configure
`CELESTIA_STATUS_PORT` through a systemd drop-in):

```bash
curl http://127.0.0.1:9080/api/v1/health
curl http://127.0.0.1:9080/api/v1/status
curl http://127.0.0.1:9080/api/v1/progress
```

This is intentionally separate from an application health endpoint such as
Auth API. It must remain loopback-only until an authenticated gateway proxies
it for a remote dashboard.

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
