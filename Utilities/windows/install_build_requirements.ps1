[CmdletBinding()]
param(
  [switch]$Build
)

$ErrorActionPreference = 'Stop'
if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)) {
  throw 'Run this script in an elevated PowerShell session.'
}
if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
  throw 'winget is required to bootstrap a clean Windows host. Install App Installer, then run this script again.'
}

function Install-WingetPackage([string]$Id, [string[]]$Extra = @()) {
  & winget install --exact --id $Id --silent --accept-package-agreements --accept-source-agreements @Extra
  if ($LASTEXITCODE -ne 0) { throw "winget failed installing $Id (exit code $LASTEXITCODE)." }
}

Install-WingetPackage 'Git.Git'
Install-WingetPackage 'Kitware.CMake'
Install-WingetPackage 'Kitware.Ninja'
Install-WingetPackage 'Microsoft.VisualStudio.2022.BuildTools' @(
  '--override', '--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
)

Write-Host 'Build requirements installed. Close and reopen PowerShell so CMake/Git PATH changes are visible.'
if ($Build) {
  $repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
  & (Join-Path $repoRoot 'Utilities\generate_and_build_windows.ps1') -Action all -Config Development -Force
}
