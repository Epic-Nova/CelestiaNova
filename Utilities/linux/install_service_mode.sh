#!/usr/bin/env bash

# Install a pre-built Celestia Nova Linux package as the hardened systemd
# service. Run this script as root (for example through sudo); it never builds
# source code and never copies developer .env files or KeyForge vault data.
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
    printf 'Run this installer through sudo.\n' >&2
    exit 1
fi

PACKAGE_ROOT="${1:-}"
INSTALL_ROOT="${2:-/opt/celestianova}"
SERVICE_USER="celestianova"

if [[ -z "${PACKAGE_ROOT}" || ! -d "${PACKAGE_ROOT}" ]]; then
    printf 'Usage: sudo %s <package-root> [install-root]\n' "$0" >&2
    exit 1
fi

if [[ ! -x "${PACKAGE_ROOT}/bin/CelestiaNova" ]]; then
    printf 'Package does not contain bin/CelestiaNova: %s\n' "${PACKAGE_ROOT}" >&2
    exit 1
fi

UNIT_SOURCE="${PACKAGE_ROOT}/share/celestianova/systemd/celestianova.service"
if [[ ! -f "${UNIT_SOURCE}" ]]; then
    printf 'Package does not contain the Celestia systemd unit: %s\n' "${UNIT_SOURCE}" >&2
    exit 1
fi

if [[ "${INSTALL_ROOT}" != /opt/celestianova ]]; then
    printf 'Only the standard install root /opt/celestianova is currently supported.\n' >&2
    exit 1
fi

if ! id -u "${SERVICE_USER}" >/dev/null 2>&1; then
    useradd --system --home /var/lib/celestianova --shell /usr/sbin/nologin "${SERVICE_USER}"
fi

systemctl stop celestianova 2>/dev/null || true
install -d -m 0755 "${INSTALL_ROOT}"
cp -a "${PACKAGE_ROOT}/." "${INSTALL_ROOT}/"
chown -R root:root "${INSTALL_ROOT}"
install -d -o "${SERVICE_USER}" -g "${SERVICE_USER}" -m 0750 "${INSTALL_ROOT}/Content/Logs"
install -D -m 0644 "${UNIT_SOURCE}" /etc/systemd/system/celestianova.service

systemctl daemon-reload
systemctl enable --now celestianova
systemctl --no-pager --full status celestianova

printf 'Celestia Nova service installed. Status: /var/lib/celestianova/status/service-status.json\n'
