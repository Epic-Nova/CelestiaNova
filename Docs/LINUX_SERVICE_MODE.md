# Linux service mode

Celestia Nova can run without CanvasCore's interactive menu as a supervised Linux service. It loads the normal extension set, runs the shared ticker, and writes an atomic status snapshot for the status API and dashboard layer.

The service mode is intentionally **not** an HTTP server. NovaAPIService or a future authenticated service gateway must expose the snapshot. This avoids accidentally publishing an unauthenticated control plane from a host daemon.

## Runtime contract

The declarative contract is [LinuxServiceMode.json](../Content/ServiceMode/LinuxServiceMode.json). The process accepts:

- `--service-mode` (or the compatibility alias `--daemon`): start headless service mode.
- `--status-file <path>`: output JSON path; default `Runtime/status/service-status.json`.
- `--status-interval-seconds <5-3600>`: heartbeat interval; default `15`.

Snapshots are written to a temporary sibling and renamed atomically. They contain only the existing `StatusApiSurface` extension lifecycle/health payload; KeyForge material, access tokens, and runtime secrets are excluded.

## Install on a VM

Install a packaged Linux build under `/opt/celestianova`, then run:

```bash
sudo useradd --system --home /var/lib/celestianova --shell /usr/sbin/nologin celestianova
sudo install -D -m 0644 /opt/celestianova/Utilities/linux/celestianova.service /etc/systemd/system/celestianova.service
sudo systemctl daemon-reload
sudo systemctl enable --now celestianova
```

Operational lifecycle:

```bash
sudo systemctl status celestianova
sudo systemctl restart celestianova
journalctl -u celestianova -f
cat /var/lib/celestianova/status/service-status.json
```

`systemd` sends `SIGTERM` on stop; Celestia Nova emits one final status snapshot and exits cleanly. The unit uses a dedicated non-login account and only permits writes to `/var/lib/celestianova`.

## Status API/dashboard integration

The consumer reads `/var/lib/celestianova/status/service-status.json` through the already-authenticated Nova API/status surface. It must report the file modification time as the daemon heartbeat and mark it stale after at least twice `statusIntervalSeconds`. It must not use a missing file as proof that a managed application is healthy.

Managed application health belongs in the owning orchestrator's extension health snapshot; the daemon aggregates it, rather than probing or starting arbitrary applications itself.
