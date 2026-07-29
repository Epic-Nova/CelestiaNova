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

## Windows

Windows has two architecture profiles: `Development` and `Shipping`
(`Shipping` maps to CMake `Release`).

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
