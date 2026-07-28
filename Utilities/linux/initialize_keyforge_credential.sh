#!/usr/bin/env bash
# Creates one systemd-encrypted KeyForge credential. Run interactively as root.
set -euo pipefail
if [[ ${EUID} -ne 0 || $# -ne 1 ]]; then echo "Usage: sudo $0 <keyforge-reference>" >&2; exit 64; fi
ref="$1"
name="${ref#keyforge://}"; name="${name//\//-}"
[[ "$ref" == keyforge://* && "$name" =~ ^[A-Za-z0-9._-]+$ ]] || { echo "Invalid KeyForge reference." >&2; exit 64; }
install -d -o root -g root -m 0700 /etc/celestianova/credentials
read -r -s -p "Secret value: " secret; echo
printf %s "$secret" | systemd-creds encrypt --name="$name" - "/etc/celestianova/credentials/$name.cred"
chmod 0600 "/etc/celestianova/credentials/$name.cred"
unset secret
echo "Credential stored. Restart celestianova.service."
