Build and Generate Scripts
==========================

This file documents the helper scripts under `Utilities/` to generate, build, install, and run the FutureLooking host tools across platforms.

Windows profile note:

- Windows helper script now uses two profiles only: `Development` and `Shipping`.
- `Shipping` is mapped to CMake `Release` in the current multi-config generator flow.

Script locations

- `Utilities/generate_and_build_linux.sh`
- `Utilities/generate_and_build_macos.sh`
- `Utilities/generate_and_build_windows.ps1`

What these scripts do

- Handle stale or unreadable `CMakeCache.txt` (prompt, or auto-clean with `--force` / `-Force`)
- Provide `generate`, `build`, and `all` actions
- Optionally run `cmake --install`
- Optionally produce a shipping package zip bundle (`-Action package`)
- Auto-generate extension `Source/CMakeLists.txt` files from extension JSON descriptors
- Enable extension dependency autowiring for build ordering and include propagation

Extension Descriptor Build Integration

During `generate`, scripts can run:

- `Utilities/generate_extension_cmakelists.cmake` to create/update extension `Source/CMakeLists.txt`
- CMake configuration with `NOVA_ENABLE_EXTENSION_DEPENDENCY_AUTOWIRE`

Control flags:

- Linux/macOS:
  - `--generate-extension-cmake <ON|OFF>`
  - `--resolve-extension-deps <ON|OFF>`
- Windows:
  - `-GenerateExtensionCMake <ON|OFF>`
  - `-ResolveExtensionDeps <ON|OFF>`

Default for both flags is `ON`.

Build Examples

Linux

Generate only:

```bash
cd FutureLooking
bash Utilities/generate_and_build_linux.sh \
  --action generate \
  --build-dir Intermediate/Build \
  --generator Ninja \
  --config Release
```

Generate + build + install:

```bash
bash Utilities/generate_and_build_linux.sh \
  --action all \
  --build-dir Intermediate/Build \
  --config Release \
  --install \
  --install-prefix ./Intermediate/Install \
  --force
```

Generate with extension CMake generation disabled:

```bash
bash Utilities/generate_and_build_linux.sh \
  --action generate \
  --generate-extension-cmake OFF
```

macOS

Use the same arguments as Linux, replacing the script name:

```bash
bash Utilities/generate_and_build_macos.sh --action all --config Release
```

Windows (PowerShell)

Generate only:

```powershell
cd FutureLooking
.\Utilities\generate_and_build_windows.ps1 \
  -Action generate \
  -BuildDir Intermediate/Solution \
  -Generator "Visual Studio 17 2022" \
  -Config Development
```

Generate with Shipping profile semantics (maps to CMake `Release`):

```powershell
.\Utilities\generate_and_build_windows.ps1 \
  -Action generate \
  -BuildDir Intermediate/Solution-Shipping \
  -Config Shipping
```

Generate + build + install:

```powershell
.\Utilities\generate_and_build_windows.ps1 \
  -Action all \
  -BuildDir Intermediate/Solution \
  -Config Development \
  -Force \
  -Install \
  -InstallPrefix C:\temp\FutureLookingInstall
```

Generate + build + shipping package zip:

```powershell
.\Utilities\generate_and_build_windows.ps1 \
  -Action package \
  -BuildDir Intermediate/Solution-Shipping \
  -PackageConfig Shipping \
  -GenerateExtensionCMake ON \
  -ResolveExtensionDeps ON
```

Generate + build + development package zip:

```powershell
.\Utilities\generate_and_build_windows.ps1 \
  -Action package \
  -BuildDir Intermediate/Solution-DevPackage \
  -PackageConfig Development \
  -GenerateExtensionCMake ON \
  -ResolveExtensionDeps ON
```

Package notes:

- `-Action package` creates a zip under `Binaries/Packages` by default.
- `-PackageConfig` accepts `Development` or `Shipping` (default: `Shipping`).
- Package includes `Binaries`, `Content`, and `Extensions`.
- Package excludes extension `Source` and `Intermediate` trees.
- `Shipping` is mapped to CMake `Release` configuration for the current multi-config setup.
- Package zip names include the selected package profile (`CelestiaNova-Development-*` or `CelestiaNova-Shipping-*`).

Full clean rebuild (Windows):

```powershell
cd FutureLooking
Remove-Item -Recurse -Force .\Intermediate, .\Binaries
.\Utilities\generate_and_build_windows.ps1 \
  -Action all \
  -BuildDir Intermediate/Solution-Clean \
  -Config Development \
  -GenerateExtensionCMake ON \
  -ResolveExtensionDeps ON
```

Full clean rebuild with Shipping profile semantics:

```powershell
cd FutureLooking
Remove-Item -Recurse -Force .\Intermediate, .\Binaries
.\Utilities\generate_and_build_windows.ps1 \
  -Action all \
  -BuildDir Intermediate/Solution-CleanShipping \
  -Config Shipping \
  -GenerateExtensionCMake ON \
  -ResolveExtensionDeps ON
```

Generate with extension dependency autowire disabled:

```powershell
.\Utilities\generate_and_build_windows.ps1 \
  -Action generate \
  -ResolveExtensionDeps OFF
```

Diff Compile (Single Extension, Windows)

When you only changed one extension and want to compile just that extension:

```powershell
cd FutureLooking
.\Utilities\build_extension_windows.ps1 \
  -ExtensionRelPath "Samples/ExampleDependentPlugin" \
  -Config Development
```

Notes:

- This script builds the extension directly from its generated plugin build tree under `Intermediate/Plugins/...`.
- Run `generate_and_build_windows.ps1 -Action generate` first so plugin build trees exist.
- Use `-IntermediatePlatformDirName` if you have more than one Windows plugin platform directory.

Run the Tools

After a successful build, host binaries are emitted to `Binaries/`.

Run Celestia Nova:

- Windows: `./Binaries/CelestiaNova.exe`
- Linux/macOS: `./Binaries/CelestiaNova`

Run PluginTester:

- Windows: `./Binaries/PluginTester.exe`
- Linux/macOS: `./Binaries/PluginTester`

If you built with `--install` / `-Install`, installed executables are under `<install-prefix>/bin`.

Notes

- `cmake --install` uses top-level `install()` rules and produces a clean install tree (typically `bin/`, `lib/`, `Content/`, `Extensions/`).
- The old `Compile.sh` flow is replaced by these per-OS scripts.
- Keep separate build directories when switching between native Windows and WSL/Linux toolchains.

Troubleshooting

- If `CMakeCache.txt exists but is not readable`, run with `--force` / `-Force` or remove that build directory from the same environment/toolchain you plan to use.
- If extension include propagation or ordering appears incorrect, verify each descriptor has a unique `id` and valid `dependencies` values (see `Extensions/JSON_SCHEMA.md`).
- If `Shipping` profile link fails, verify all third-party libraries are available as Release-compatible builds for the active toolchain.
