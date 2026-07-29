#!/usr/bin/env bash
# Create a signed, portable Celestia Nova update artifact for static hosting.
set -euo pipefail

PACKAGE_ROOT="${1:?Usage: package_update_artifact.sh <package-root> <version> <private-key.pem> [output-dir]}"
VERSION="${2:?Missing version}"
PRIVATE_KEY="${3:?Missing private signing key}"
OUTPUT_DIR="${4:-$(pwd)/Artifacts/Updates}"

for command in zip sha256sum openssl; do command -v "$command" >/dev/null || { echo "Missing $command" >&2; exit 1; }; done
[[ -x "$PACKAGE_ROOT/bin/CelestiaNova" ]] || { echo "Invalid package root" >&2; exit 1; }
[[ -f "$PRIVATE_KEY" ]] || { echo "Signing key not found" >&2; exit 1; }
[[ "$VERSION" =~ ^[0-9A-Za-z._-]+$ ]] || { echo "Unsafe version" >&2; exit 1; }

mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR="$(cd "$OUTPUT_DIR" && pwd)"
BASE="celestianova-${VERSION}-linux"
ARCHIVE="$OUTPUT_DIR/${BASE}.zip"
SIGNATURE="$ARCHIVE.sig"
MANIFEST="$OUTPUT_DIR/${BASE}.manifest.json"
PUBLIC_KEY="$OUTPUT_DIR/${BASE}.public.pem"
rm -f "$ARCHIVE" "$SIGNATURE" "$MANIFEST" "$PUBLIC_KEY"

(cd "$(dirname "$PACKAGE_ROOT")" && zip -qr "$ARCHIVE" "$(basename "$PACKAGE_ROOT")")
SHA256="$(sha256sum "$ARCHIVE" | awk '{print $1}')"
openssl dgst -sha256 -sign "$PRIVATE_KEY" -out "$SIGNATURE" "$ARCHIVE"
openssl pkey -in "$PRIVATE_KEY" -pubout -out "$PUBLIC_KEY"

cat > "$MANIFEST" <<EOF
{
  "channel": "stable",
  "version": "$VERSION",
  "package_url": "REPLACE_WITH_HTTPS_URL/${BASE}.zip",
  "sha256": "$SHA256",
  "signature_url": "REPLACE_WITH_HTTPS_URL/${BASE}.zip.sig"
}
EOF
echo "Created: $ARCHIVE"
echo "Created: $SIGNATURE"
echo "Created: $PUBLIC_KEY (install once as /etc/celestianova/update-trust.pem)"
echo "Edit URLs in: $MANIFEST before placing its values in Auth API."
