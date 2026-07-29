# Celestia Nova — Build and service guide

This repository contains the Celestia Nova host, its extensions, the `celest`
command surface, and the Linux service-mode package. Build helpers generate
extension CMake projects from JSON descriptors before compiling.

## Linux production package (VM/staging)

This is the authoritative command for the VM hosting slice. It configures,
builds, and creates the relocatable production package:

```bash
cd ~/CelestiaNova
git pull --ff-only
sudo bash Utilities/linux/install_build_requirements.sh
bash Utilities/linux/build_and_package_linux.sh
```

The resulting package is:

```text
Artifacts/CelestiaNova-Linux-Production
```

Install or update the daemon from that package:

```bash
sudo bash Utilities/linux/install_service_mode.sh \
  ~/CelestiaNova/Artifacts/CelestiaNova-Linux-Production
```

Useful verification commands:

```bash
systemctl status celestianova
celest status
celest progress
curl http://127.0.0.1:9080/api/v1/health
curl http://127.0.0.1:9080/api/v1/status
```

`celest` without arguments opens the interactive FTXUI console. Scripted use
remains available through commands such as `celest deploy auth-api minimal`,
`celest run --mesh-status`, and `celest complete de`.

For the local Nova ID login/bootstrap sequence after an Auth API deployment,
see [Docs/KEYFORGE.md](Docs/KEYFORGE.md#local-nova-id-bootstrap-flow). It
creates the explicit local-only `Admin` identity and shows the Postman device
approval request; no password is used by that test-only bypass flow.

## Signed daemon updates

Build the normal Linux package first, then create a signed update artifact:

```bash
bash Utilities/linux/package_update_artifact.sh \
  Artifacts/CelestiaNova-Linux-Production 1.0.0 /secure/path/update-signing-key.pem
```

It produces a ZIP, detached signature, SHA-256 manifest template, and public
key in `Artifacts/Updates`. Host the ZIP and `.sig` over HTTPS, put their URLs
and the SHA-256 into Auth API's update manifest, and install the generated
public key once on each daemon:

```bash
sudo install -D -m 0644 Artifacts/Updates/celestianova-1.0.0-linux.public.pem \
  /etc/celestianova/update-trust.pem
```

`celest run --check-updates` then asks SyncForge to fetch the protected
manifest. A verified package is applied in a separate root-owned systemd unit,
so stopping the old daemon cannot interrupt its own upgrade.

## Windows

Windows has two architecture profiles: `Development` and `Shipping`
(`Shipping` maps to CMake `Release`).

On a clean elevated Windows host, install prerequisites first:

```powershell
.\Utilities\windows\install_build_requirements.ps1
```

Generate only:

```powershell
cd FutureLooking
.\Utilities\generate_and_build_windows.ps1 `
  -Action generate `
  -BuildDir Intermediate/Solution `
  -Generator "Visual Studio 17 2022" `
  -Config Development
```

Generate and build:

```powershell
.\Utilities\generate_and_build_windows.ps1 `
  -Action all `
  -BuildDir Intermediate/Solution `
  -Config Development `
  -Force
```

Package a shipping build:

```powershell
.\Utilities\generate_and_build_windows.ps1 `
  -Action package `
  -BuildDir Intermediate/Solution-Shipping `
  -PackageConfig Shipping
```

## Other native builds

The generic helpers support generate, build, and install actions:

```bash
bash Utilities/generate_and_build_linux.sh --action all --config Release
bash Utilities/generate_and_build_macos.sh --action all --config Release
```

All helpers generate extension `Source/CMakeLists.txt` files and resolve
descriptor dependencies by default. Pass the relevant
`generate-extension-cmake` or `resolve-extension-deps` option only when
debugging generator behaviour.

## Single extension build (Windows)

After a project generation, rebuild one extension directly:

```powershell
.\Utilities\build_extension_windows.ps1 `
  -ExtensionRelPath "Samples/ExampleDependentPlugin" `
  -Config Development
```

## Troubleshooting

- Keep separate build directories for Windows and Linux/WSL toolchains.
- A stale or unreadable `CMakeCache.txt` can be regenerated with `--force`
  (Linux/macOS) or `-Force` (Windows).
- If extension ordering or includes are wrong, check that every extension
  descriptor has a unique `id`, a module `file`, and valid `dependencies`.
- Never remove broad project directories for a rebuild; target only the
  relevant `Intermediate` build directory.

## Documentation and backlog

- [TODO.md](TODO.md) is the single source of unfinished work.
- `Docs/` contains only stable operating and architecture contracts: Linux
  service mode, KeyForge, SyncForge updates, and declarative infrastructure.
