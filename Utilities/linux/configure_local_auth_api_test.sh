#!/usr/bin/env bash
# Explicitly opt a Linux Celestia test node into its co-located HTTP Auth API.
# Production stays HTTPS-only unless this root-owned local-test configuration
# exists. Credentials are intentionally not stored here.
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  printf 'Run this configurator through sudo.\n' >&2
  exit 1
fi

BASE_URL="${1:-http://127.0.0.1:8081}"
PUBLIC_BASE_URL="${2:-}"
if [[ ! "${BASE_URL}" =~ ^http://(127\.0\.0\.1|localhost)(:[0-9]{1,5})?$ ]]; then
  printf 'Only a loopback HTTP Auth API URL is accepted for local test mode.\n' >&2
  exit 1
fi
if [[ -n "${PUBLIC_BASE_URL}" && ! "${PUBLIC_BASE_URL}" =~ ^https?://[^[:space:]/:]+(:[0-9]{1,5})?$ ]]; then
  printf 'The optional public Auth API URL must be an http(s) host and port without a path.\n' >&2
  exit 1
fi

install -d -o root -g root -m 0755 /etc/celestianova
umask 022
cat > /etc/celestianova/runtime.env <<EOF
CELESTIA_AUTH_API_BASE_URL=${BASE_URL}
CELESTIA_LOCAL_TEST_MODE=1
EOF
if [[ -n "${PUBLIC_BASE_URL}" ]]; then
  printf 'CELESTIA_AUTH_API_PUBLIC_BASE_URL=%s\n' "${PUBLIC_BASE_URL}" >> /etc/celestianova/runtime.env
fi
chown root:root /etc/celestianova/runtime.env
chmod 0644 /etc/celestianova/runtime.env
systemctl daemon-reload
systemctl restart celestianova.service
printf 'Local-test Auth API routing enabled for %s.\n' "${BASE_URL}"
