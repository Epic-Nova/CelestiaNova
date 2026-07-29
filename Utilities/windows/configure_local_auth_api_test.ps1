param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^http://[^/\s:]+(:\d{1,5})?$')]
    [string]$BaseUrl
)

# Explicit opt-in for a Windows Celestia client that talks to a host-only VM
# Auth API. This stores endpoint routing only; it never stores credentials.
[Environment]::SetEnvironmentVariable('CELESTIA_AUTH_API_BASE_URL', $BaseUrl, 'User')
[Environment]::SetEnvironmentVariable('CELESTIA_LOCAL_TEST_MODE', '1', 'User')
$env:CELESTIA_AUTH_API_BASE_URL = $BaseUrl
$env:CELESTIA_LOCAL_TEST_MODE = '1'
Write-Host "Local Auth API test routing enabled for $BaseUrl. Restart Celestia Nova if it is already running."
