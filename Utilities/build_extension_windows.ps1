<#
build_extension_windows.ps1

Compile exactly one extension plugin from its generated Intermediate/Plugins build tree.
This is useful for diff/incremental builds when only one extension changed.

Usage:
  .\build_extension_windows.ps1 -ExtensionRelPath "Samples/ExampleDependentPlugin" [-Config Development] [-IntermediatePlatformDirName windows-visual_studio_17_2022-development]
#>

param(
  [Parameter(Mandatory = $true)]
  [string]$ExtensionRelPath,
  [string]$Config = 'Development',
  [string]$IntermediatePlatformDirName = ''
)

$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptDir '..')).Path

# Normalize to "Folder/SubFolder" relative to Extensions root.
$normalized = $ExtensionRelPath.Replace('\\', '/')
if ($normalized.StartsWith('Extensions/')) {
  $normalized = $normalized.Substring('Extensions/'.Length)
}
$normalized = $normalized.Trim('/')

if ([string]::IsNullOrWhiteSpace($normalized)) {
  throw 'ExtensionRelPath cannot be empty.'
}

$extensionRoot = Join-Path $RepoRoot ("Extensions\" + ($normalized -replace '/', '\\'))
if (-not (Test-Path $extensionRoot)) {
  throw "Extension folder does not exist: $extensionRoot"
}

# Match CMakeLists.txt hashing logic:
# string(MD5 _plugin_path_hash "${p}")
# string(SUBSTRING "${_plugin_path_hash}" 0 12 _plugin_path_hash_short)
$md5 = [System.Security.Cryptography.MD5]::Create()
$bytes = [System.Text.Encoding]::UTF8.GetBytes($normalized)
$hashBytes = $md5.ComputeHash($bytes)
$hash = -join ($hashBytes | ForEach-Object { $_.ToString('x2') })
$hashShort = $hash.Substring(0, 12)

$pluginsRoot = Join-Path $RepoRoot 'Intermediate\Plugins'
if (-not (Test-Path $pluginsRoot)) {
  throw "Plugin intermediate root not found: $pluginsRoot. Run generate_and_build_windows.ps1 -Action generate first."
}

if ([string]::IsNullOrWhiteSpace($IntermediatePlatformDirName)) {
  $candidates = @(Get-ChildItem -Path $pluginsRoot -Directory | Select-Object -ExpandProperty Name)
  if (-not $candidates) {
    throw "No intermediate platform directories found under $pluginsRoot. Run generate first."
  }

  $winCandidates = @($candidates | Where-Object { $_ -like 'windows-*' })
  if ($winCandidates.Count -eq 1) {
    $IntermediatePlatformDirName = $winCandidates[0]
  } elseif ($winCandidates.Count -gt 1) {
    throw "Multiple Windows platform dirs found ($($winCandidates -join ', ')). Pass -IntermediatePlatformDirName explicitly."
  } elseif ($candidates.Count -eq 1) {
    $IntermediatePlatformDirName = $candidates[0]
  } else {
    throw "Multiple platform dirs found ($($candidates -join ', ')). Pass -IntermediatePlatformDirName explicitly."
  }
}

$pluginBuildDir = Join-Path $pluginsRoot (Join-Path $IntermediatePlatformDirName $hashShort)
if (-not (Test-Path $pluginBuildDir)) {
  throw "Plugin build dir not found: $pluginBuildDir. Run generate first and confirm the extension path is correct ($normalized)."
}

Write-Host "Building extension: $normalized"
Write-Host "Plugin build dir: $pluginBuildDir"

& cmake --build $pluginBuildDir --config $Config
if ($LASTEXITCODE -ne 0) {
  throw "Extension build failed for '$normalized'."
}

Write-Host "Done."
