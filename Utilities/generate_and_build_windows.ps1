<#
generate_and_build_windows.ps1
Usage:
  .\generate_and_build_windows.ps1 -Action generate|build|all|package [-BuildDir "Intermediate/Solution"] [-Generator "Visual Studio 17 2022"] [-Config Development|Shipping] [-PackageConfig Development|Shipping] [-GenerateExtensionCMake ON|OFF] [-ResolveExtensionDeps ON|OFF] [-Force] [-Install] [-InstallPrefix C:\path\to\install] [-PackageOutputDir Binaries/Packages]

This script:
 - handles stale/unreadable CMakeCache.txt by prompting or removing with -Force
 - generates Visual Studio solution (or other generator) and builds it
 - runs cmake --install when requested
 - can produce a profile package zip containing required binaries/content/extensions without source/intermediate trees
#>
param(
  [ValidateSet('generate','build','all','package')]
  [string]$Action = 'all',
  [string]$Generator = 'Visual Studio 17 2022',
  [string]$BuildDir = 'Intermediate/Solution',
  [ValidateSet('Development','Shipping')]
  [string]$Config = 'Development',
  [ValidateSet('Development','Shipping')]
  [string]$PackageConfig = 'Shipping',
  [ValidateSet('ON','OFF')]
  [string]$GenerateExtensionCMake = 'ON',
  [ValidateSet('ON','OFF')]
  [string]$ResolveExtensionDeps = 'ON',
  [switch]$Force,
  [switch]$Install,
  [string]$InstallPrefix = '',
  [string]$PackageOutputDir = 'Binaries/Packages'
)

$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir '..')
$RepoRoot = $RepoRoot.Path
$FullBuildDir = Join-Path $RepoRoot $BuildDir

function Resolve-CMakeConfiguration([string]$Profile) {
  if ($Profile -eq 'Shipping') {
    return 'Release'
  }
  return 'Development'
}

$SelectedProfile = if ($Action -eq 'package') { $PackageConfig } else { $Config }
$EffectiveBuildConfig = Resolve-CMakeConfiguration -Profile $SelectedProfile
if ($SelectedProfile -eq 'Shipping') {
  Write-Host "Selected profile '$SelectedProfile': mapped to CMake '$EffectiveBuildConfig' configuration."
} else {
  Write-Host "Selected profile '$SelectedProfile': using CMake '$EffectiveBuildConfig' configuration."
}

function Usage {
  Write-Host "Usage: .\generate_and_build_windows.ps1 -Action generate|build|all|package [-BuildDir <dir>] [-Generator <cmake generator>] [-Config <Development|Shipping>] [-PackageConfig <Development|Shipping>] [-GenerateExtensionCMake <ON|OFF>] [-ResolveExtensionDeps <ON|OFF>] [-Force] [-Install] [-InstallPrefix <dir>] [-PackageOutputDir <dir>]"
}

function Invoke-RobocopyCopy {
  param(
    [Parameter(Mandatory=$true)][string]$Source,
    [Parameter(Mandatory=$true)][string]$Destination,
    [string[]]$Patterns = @('*.*'),
    [switch]$Recursive,
    [string[]]$ExcludeDirs = @(),
    [string[]]$ExcludeFiles = @()
  )

  if (-not (Test-Path $Source)) {
    return
  }

  if (-not (Test-Path $Destination)) {
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
  }

  $robocopyArgs = @($Source, $Destination)
  $robocopyArgs += $Patterns
  if ($Recursive) {
    $robocopyArgs += '/E'
  }
  if ($ExcludeDirs.Count -gt 0) {
    $robocopyArgs += '/XD'
    $robocopyArgs += $ExcludeDirs
  }
  if ($ExcludeFiles.Count -gt 0) {
    $robocopyArgs += '/XF'
    $robocopyArgs += $ExcludeFiles
  }
  $robocopyArgs += @('/R:1', '/W:1', '/NFL', '/NDL', '/NJH', '/NJS', '/NP')

  & robocopy @robocopyArgs | Out-Null
  if ($LASTEXITCODE -ge 8) {
    throw "Robocopy failed copying from '$Source' to '$Destination' (exit code $LASTEXITCODE)."
  }
}

function Generate-ExtensionCMakeLists {
  if ($GenerateExtensionCMake -ne 'ON') {
    Write-Host "Skipping extension CMakeLists generation (GenerateExtensionCMake=$GenerateExtensionCMake)"
    return
  }

  $generatorScript = Join-Path $ScriptDir 'generate_extension_cmakelists.cmake'
  if (-not (Test-Path $generatorScript)) {
    Write-Warning "Extension CMake generator script not found: $generatorScript"
    return
  }

  Write-Host "Generating extension Source/CMakeLists.txt files from JSON descriptors"
  & cmake "-DREPO_ROOT=$RepoRoot" -P $generatorScript
  if ($LASTEXITCODE -ne 0) {
    throw "Failed to generate extension CMakeLists files"
  }
}

function Ensure-BuildDir {
  if (Test-Path (Join-Path $FullBuildDir 'CMakeCache.txt')) {
    try {
      Get-Content (Join-Path $FullBuildDir 'CMakeCache.txt') -ErrorAction Stop | Out-Null
    } catch {
      Write-Host "CMakeCache.txt exists but is not readable: $FullBuildDir\CMakeCache.txt"
      if ($Force) {
        if ($BuildDir -eq '.') {
          Write-Host "Forcing clean of root CMake artifacts..."
          if (Test-Path (Join-Path $RepoRoot "CMakeCache.txt")) { Remove-Item -Force (Join-Path $RepoRoot "CMakeCache.txt") }
          if (Test-Path (Join-Path $RepoRoot "CMakeFiles")) { Remove-Item -Recurse -Force (Join-Path $RepoRoot "CMakeFiles") }
        } else {
          Write-Host "Forcing clean of build directory: $FullBuildDir"
          if (Test-Path $FullBuildDir) { Remove-Item -Recurse -Force $FullBuildDir }
        }
        New-Item -ItemType Directory -Path $FullBuildDir | Out-Null
      } else {
        $ans = Read-Host "Remove build directory and recreate it? (y/N)"
        if ($ans -match '^[Yy]') {
          Remove-Item -LiteralPath $FullBuildDir -Recurse -Force
          New-Item -ItemType Directory -Path $FullBuildDir | Out-Null
        } else {
          Write-Error "Aborting due to unreadable CMakeCache.txt"
          exit 1
        }
      }
    }
  } else {
    if (-not (Test-Path $FullBuildDir)) { New-Item -ItemType Directory -Path $FullBuildDir | Out-Null }
  }
}

function Mirror-SolutionToRoot {
    param([string]$SlnName = "CelestiaNova.sln")
    $sourceSln = Join-Path $FullBuildDir $SlnName
    $targetSln = Join-Path $RepoRoot $SlnName

    if (!(Test-Path $sourceSln)) {
        Write-Warning "Source solution not found for mirroring: $sourceSln"
        return
    }

    Write-Host "Mirroring solution to root for convenience..." -ForegroundColor Cyan
    $content = Get-Content $sourceSln -Raw
    
    # Prefix relative paths for Projects and Solution Folders
    # CMake generated projects: Project("{GUID}") = "Name", "RelativePath", "{GUID}"
    $relativeBuildPath = $BuildDir.Replace('/', '\')
    if (!$relativeBuildPath.EndsWith('\')) { $relativeBuildPath += '\' }

    $lines = $content.Split("`n")
    $newLines = @()
    foreach ($line in $lines) {
        if ($line -match '^Project\("\{[A-F0-9-]+\}"\) = "([^"]+)", "([^"]+)", "(\{[A-F0-9-]+\})"') {
            $name = $matches[1]
            $path = $matches[2]
            $uuid = $matches[3]
            $guid = ($line -split '"')[1] # Preserve original project type GUID

            # If path is relative (no : and doesn't start with build dir)
            if ($path -notmatch '^[A-Za-z]:' -and -not $path.StartsWith($relativeBuildPath)) {
                $path = Join-Path $relativeBuildPath $path
            }
            $newLines += "Project(`"$($guid)`") = `"$($name)`", `"$($path)`", `"$($uuid)`""
        } else {
            $newLines += $line
        }
    }
    
    $newContent = $newLines -join "`n"
    Set-Content -Path $targetSln -Value $newContent -Encoding UTF8
}

function Generate {
  Generate-ExtensionCMakeLists
  Write-Host "Generating project files in: $FullBuildDir"
  $cmakeArgs = @(
    "-S", $RepoRoot,
    "-B", $FullBuildDir,
    "-G", $Generator,
    "-DCMAKE_BUILD_TYPE=$EffectiveBuildConfig",
    "-DNOVA_ENABLE_EXTENSION_DEPENDENCY_AUTOWIRE=$ResolveExtensionDeps"
  )
  & cmake @cmakeArgs
  if ($LASTEXITCODE -ne 0) {
    throw "CMake generate failed (exit code $LASTEXITCODE)."
  }

  if ($Generator -match "Visual Studio") {
    Mirror-SolutionToRoot
  }
}

function Build {
  Write-Host "Building in: $FullBuildDir (config: $EffectiveBuildConfig)"
  & cmake --build $FullBuildDir --config $EffectiveBuildConfig
  if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed (exit code $LASTEXITCODE)."
  }
}

function Install-Target {
  if ($Install) {
    Write-Host "Installing from: $FullBuildDir"
    if ($InstallPrefix -ne '') {
      & cmake --install $FullBuildDir --config $EffectiveBuildConfig --prefix $InstallPrefix
    } else {
      & cmake --install $FullBuildDir --config $EffectiveBuildConfig
    }
    if ($LASTEXITCODE -ne 0) {
      throw "CMake install failed (exit code $LASTEXITCODE)."
    }
  }
}

function Package-Bundle {
  param(
    [Parameter(Mandatory=$true)][string]$PackageProfile
  )

  $packageOutputRoot = Join-Path $RepoRoot $PackageOutputDir
  if (-not (Test-Path $packageOutputRoot)) {
    New-Item -ItemType Directory -Path $packageOutputRoot -Force | Out-Null
  }

  $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
  $packageName = "CelestiaNova-$PackageProfile-$timestamp"
  $stagingParent = Join-Path ([System.IO.Path]::GetTempPath()) ("celestianova-package-" + [Guid]::NewGuid().ToString('N'))
  $stagingRoot = Join-Path $stagingParent $packageName
  New-Item -ItemType Directory -Path $stagingRoot -Force | Out-Null

  try {
    Write-Host "Staging package contents in: $stagingRoot"

    $binariesSource = Join-Path $RepoRoot 'Binaries'
    $binariesTarget = Join-Path $stagingRoot 'Binaries'
    Invoke-RobocopyCopy -Source $binariesSource -Destination $binariesTarget -Patterns @('*.exe', '*.dll', '*.json', '*.ini', '*.cfg') -Recursive

    $contentSource = Join-Path $RepoRoot 'Content'
    $contentTarget = Join-Path $stagingRoot 'Content'
    Invoke-RobocopyCopy -Source $contentSource -Destination $contentTarget -Patterns @('*.*') -Recursive

    $extensionsSource = Join-Path $RepoRoot 'Extensions'
    $extensionsTarget = Join-Path $stagingRoot 'Extensions'
    Invoke-RobocopyCopy -Source $extensionsSource -Destination $extensionsTarget -Patterns @('*.*') -Recursive -ExcludeDirs @('Source', 'Intermediate') -ExcludeFiles @('*.vcxproj', '*.vcxproj.filters', '*.user', '*.obj', '*.pdb', '*.ilk')

    $infoPath = Join-Path $stagingRoot 'PACKAGE_INFO.txt'
    $infoLines = @(
      "Celestia Nova Package",
      "GeneratedAtUtc: $([DateTime]::UtcNow.ToString('o'))",
      "BuildProfile: $PackageProfile",
      "BuildConfiguration: $EffectiveBuildConfig",
      "Contents: Binaries, Content, Extensions (without Source/Intermediate)"
    )
    Set-Content -Path $infoPath -Value $infoLines -Encoding UTF8

    $zipPath = Join-Path $packageOutputRoot ($packageName + '.zip')
    if (Test-Path $zipPath) {
      Remove-Item -LiteralPath $zipPath -Force
    }

    Compress-Archive -Path $stagingRoot -DestinationPath $zipPath -CompressionLevel Optimal
    Write-Host "Package created: $zipPath"
  }
  finally {
    if (Test-Path $stagingParent) {
      Remove-Item -LiteralPath $stagingParent -Recurse -Force
    }
  }
}

# Main
Ensure-BuildDir
if ($Action -eq 'generate') {
  Generate
} elseif ($Action -eq 'build') {
  Build
  Install-Target
} elseif ($Action -eq 'all') {
  Generate
  Build
  Install-Target
} elseif ($Action -eq 'package') {
  Generate
  Build
  Package-Bundle -PackageProfile $PackageConfig
}

Write-Host "Done. Build dir: $FullBuildDir"