[CmdletBinding()]
param(
    [string]$Reference = 'keyforge://bootstrap/auth-api/provisioning',
    [string]$VaultPath = (Join-Path $PSScriptRoot '..\Content\.runtime\keyforge-v1.dpapi')
)

$ErrorActionPreference = 'Stop'
if ($Reference -notmatch '^keyforge://[A-Za-z0-9._/-]+$') { throw 'Invalid KeyForge reference.' }
if (Test-Path -LiteralPath $VaultPath) { throw 'Vault already exists. Refusing to overwrite protected secrets.' }

Add-Type -AssemblyName System.Security
$secret = Read-Host 'Auth API KeyForge bootstrap secret' -AsSecureString
$ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secret)
try {
    $plain = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr)
    if ([string]::IsNullOrWhiteSpace($plain) -or $plain.Contains("`0") -or $plain.Contains("`r") -or $plain.Contains("`n")) {
        throw 'Bootstrap secret is empty or contains unsupported characters.'
    }
    $scope = [Security.Cryptography.DataProtectionScope]::CurrentUser
    $key = [Text.Encoding]::UTF8.GetBytes($Reference.Substring('keyforge://'.Length))
    $value = [Text.Encoding]::UTF8.GetBytes($plain)
    # Must stay null: the native KeyForge DPAPI backend uses no optional
    # entropy so a vault created here remains readable by the extension.
    $protectedKey = [Security.Cryptography.ProtectedData]::Protect($key, $null, $scope)
    $protectedValue = [Security.Cryptography.ProtectedData]::Protect($value, $null, $scope)
    $parent = Split-Path -Parent $VaultPath
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    $stream = [IO.File]::Open($VaultPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $writer = [IO.BinaryWriter]::new($stream)
        $writer.Write([Text.Encoding]::ASCII.GetBytes('KFV1DPAP'))
        $writer.Write([uint32]1)
        $writer.Write([uint32]$protectedKey.Length); $writer.Write($protectedKey)
        $writer.Write([uint32]$protectedValue.Length); $writer.Write($protectedValue)
        $writer.Flush()
    } finally { $stream.Dispose() }
} finally {
    if ($ptr -ne [IntPtr]::Zero) { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr) }
    Remove-Variable plain -ErrorAction SilentlyContinue
}

Write-Host 'KeyForge bootstrap secret protected for the current Windows user.'
