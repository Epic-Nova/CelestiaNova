#!/usr/bin/env bash

# Explicit first-run provisioning for the bundled local Auth API content pack.
# It deliberately remains separate from service installation: operators can
# review, back up, or replace KeyForge credentials without reinstalling the
# Celestia daemon. Existing credentials are never overwritten.
set -euo pipefail

if [[ "${EUID}" -ne 0 || "$#" -ne 0 ]]; then
    printf 'Usage: sudo %s\n' "$0" >&2
    exit 64
fi

command -v systemd-creds >/dev/null 2>&1 || {
    printf 'systemd-creds is required for encrypted KeyForge credentials.\n' >&2
    exit 69
}

install -d -o root -g root -m 0700 /etc/celestianova/credentials

ensure_generated_credential() {
    local source_name="$1"
    local value="$2"
    local destination="/etc/celestianova/credentials/${source_name}"
    if [[ -f "${destination}" ]]; then
        printf 'Keeping existing KeyForge credential: %s\n' "${source_name}"
        return 0
    fi
    printf %s "${value}" | systemd-creds encrypt --name="keyforge_${source_name}" - "${destination}"
    chmod 0600 "${destination}"
    printf 'Created encrypted KeyForge credential: %s\n' "${source_name}"
}

app_key="base64:$(head -c 32 /dev/urandom | base64 -w 0)"
database_password="$(od -An -N32 -tx1 /dev/urandom | tr -d ' \n')"
ensure_generated_credential "content-auth-api-app-key" "${app_key}"
ensure_generated_credential "content-auth-api-db-password" "${database_password}"
unset app_key database_password

printf 'Local Auth API KeyForge credentials are provisioned. You may now deploy auth-api.\n'
