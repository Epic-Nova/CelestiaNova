#!/usr/bin/env bash
# Starts the root update applier outside celestianova.service's cgroup so the
# package installer can safely stop and restart that daemon.
set -euo pipefail
[[ $# -eq 6 && "$1" == "--package-url" && "$3" == "--signature-url" && "$5" == "--sha256" ]] || { echo "Invalid update request" >&2; exit 64; }
for value in "$2" "$4"; do [[ "$value" =~ ^https://[^[:space:]]+$ ]] || { echo "HTTPS URLs required" >&2; exit 65; }; done
[[ "$6" =~ ^[a-fA-F0-9]{64}$ ]] || { echo "Invalid SHA-256" >&2; exit 65; }
exec /usr/bin/systemd-run --quiet --collect --unit=celestianova-syncforge-update \
  /usr/local/lib/celestianova/apply-syncforge-update "$@"
