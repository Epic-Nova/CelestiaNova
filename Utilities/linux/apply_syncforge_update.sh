#!/usr/bin/env bash
# Root-owned SyncForge applier. It accepts only a signed package and delegates
# installation to the package's existing service-mode installer.
set -euo pipefail

TRUST_KEY="/etc/celestianova/update-trust.pem"
INSTALLER="/opt/celestianova/share/celestianova/bootstrap/install_service_mode.sh"
PACKAGE_URL=""; SIGNATURE_URL=""; EXPECTED_SHA256=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --package-url) PACKAGE_URL="${2:-}"; shift 2 ;;
    --signature-url) SIGNATURE_URL="${2:-}"; shift 2 ;;
    --sha256) EXPECTED_SHA256="${2:-}"; shift 2 ;;
    *) echo "Unknown argument" >&2; exit 64 ;;
  esac
done
[[ "$PACKAGE_URL" =~ ^https://[^[:space:]]+$ && "$SIGNATURE_URL" =~ ^https://[^[:space:]]+$ ]] || { echo "HTTPS URLs required" >&2; exit 65; }
[[ "$EXPECTED_SHA256" =~ ^[a-fA-F0-9]{64}$ ]] || { echo "Invalid SHA-256" >&2; exit 65; }
[[ -r "$TRUST_KEY" ]] || { echo "Missing update trust key: $TRUST_KEY" >&2; exit 66; }
[[ -x "$INSTALLER" ]] || { echo "Missing package installer" >&2; exit 66; }
for command in curl sha256sum openssl unzip find; do command -v "$command" >/dev/null || { echo "Missing $command" >&2; exit 67; }; done

WORK="$(mktemp -d /var/lib/celestianova/update.XXXXXX)"
cleanup() { rm -rf "$WORK"; }
trap cleanup EXIT
curl --fail --silent --show-error --location --proto '=https' --tlsv1.2 "$PACKAGE_URL" -o "$WORK/update.zip"
curl --fail --silent --show-error --location --proto '=https' --tlsv1.2 "$SIGNATURE_URL" -o "$WORK/update.zip.sig"
ACTUAL_SHA256="$(sha256sum "$WORK/update.zip" | awk '{print $1}')"
[[ "${ACTUAL_SHA256,,}" == "${EXPECTED_SHA256,,}" ]] || { echo "Package checksum mismatch" >&2; exit 68; }
openssl dgst -sha256 -verify "$TRUST_KEY" -signature "$WORK/update.zip.sig" "$WORK/update.zip" >/dev/null
unzip -q "$WORK/update.zip" -d "$WORK/unpacked"
PACKAGE_ROOT="$(find "$WORK/unpacked" -type f -path '*/bin/CelestiaNova' -printf '%h\n' | head -n 1 | xargs -r dirname)"
[[ -n "$PACKAGE_ROOT" && -x "$PACKAGE_ROOT/bin/CelestiaNova" ]] || { echo "Invalid update package layout" >&2; exit 69; }
exec "$INSTALLER" "$PACKAGE_ROOT"
