# SyncForge signed daemon updates

This runbook describes the complete Celestia Nova Linux daemon update path.
SyncForge is the only extension that owns update discovery and installation.
KeyForge owns OAuth credentials, Auth API publishes the update manifest, and
the root-owned systemd applier performs the installation.

## Trust model

```text
release package → signed ZIP + SHA-256 → HTTPS artifact host
     → Auth API manifest → KeyForge OAuth request → SyncForge
     → isolated root systemd update unit → verified package install
```

- The update signing private key never enters Celestia Nova, Auth API, or the
  artifact host.
- Each daemon trusts only `/etc/celestianova/update-trust.pem`.
- KeyForge obtains the short-lived Auth API access token internally; neither
  OAuth client secrets nor access tokens are written to Content, status data,
  CLI output, or logs.
- The root update helper accepts only HTTPS URLs, a 64-character SHA-256 value,
  and a detached signature that verifies with the local trust key.

## 1. Build a production package

On Linux:

```bash
cd ~/CelestiaNova
git pull --ff-only
bash Utilities/linux/build_and_package_linux.sh
```

This creates `Artifacts/CelestiaNova-Linux-Production`.

## 2. Create an update artifact

Use an offline or otherwise protected signing key:

```bash
bash Utilities/linux/package_update_artifact.sh \
  Artifacts/CelestiaNova-Linux-Production \
  1.0.0 \
  /secure/path/update-signing-key.pem
```

The command creates these files under `Artifacts/Updates`:

- `celestianova-1.0.0-linux.zip`
- `celestianova-1.0.0-linux.zip.sig`
- `celestianova-1.0.0-linux.manifest.json`
- `celestianova-1.0.0-linux.public.pem`

Do not upload the private key. Upload only the ZIP and detached signature to
an HTTPS artifact location. Edit the generated manifest template so its two
URLs point at those hosted files.

## 3. Bootstrap daemon trust

Install the generated public key once on every daemon before it may apply
updates:

```bash
sudo install -D -m 0644 \
  Artifacts/Updates/celestianova-1.0.0-linux.public.pem \
  /etc/celestianova/update-trust.pem
```

Key rotation is an explicit deployment: ship a package signed by the currently
trusted key that contains/configures the next trust key, then switch the local
trust file under administrator control.

## 4. Publish through Auth API

Auth API serves the versioned `celestia-instances/update-manifest` capability.
Set its static update configuration to the desired version, package URL,
SHA-256, and signature URL. The package and signature URLs must use HTTPS.

Register and provision the `celestianova-syncforge` OAuth application with the
`celestia.update.read` scope. Put its client ID and secret only in KeyForge's
protected backend (Linux systemd encrypted credentials or Windows DPAPI).

## 5. Check and apply

On a daemon:

```bash
celest run --check-updates
celest progress
```

SyncForge obtains the manifest through KeyForge. If it is structurally valid,
it queues `celestianova-syncforge-update`, a root-owned transient systemd unit.
That unit downloads the ZIP and signature to a private temporary directory,
checks the SHA-256, verifies the signature with
`/etc/celestianova/update-trust.pem`, validates the package layout, and runs
the package-owned service installer. The installer stops and restarts
`celestianova.service`; the update unit is outside that service cgroup and
therefore is not interrupted by the restart.

Observe it with:

```bash
systemctl status celestianova-syncforge-update
journalctl -u celestianova-syncforge-update -f
systemctl status celestianova
```

## Failure behaviour

The update fails closed and leaves the current installation running when the
OAuth lease is unavailable, the manifest is malformed, HTTPS fails, the hash
does not match, the signature is invalid, the trust key is missing, or the ZIP
does not contain a valid Celestia Nova package. A failed update must be fixed
and republished; SyncForge never executes an unverified artifact.
