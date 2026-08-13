$ErrorActionPreference = "Stop"
$HostRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Python = Join-Path $HostRoot ".venv\Scripts\python.exe"

if (-not (Test-Path $Python)) {
    py -3 -m venv (Join-Path $HostRoot ".venv")
}
& $Python -m pip install --disable-pip-version-check -r (Join-Path $HostRoot "requirements.txt")

$EnvPath = Join-Path $HostRoot ".env"
if (-not (Test-Path $EnvPath)) {
    $Template = Get-Content (Join-Path $HostRoot ".env.example") -Raw
    $Token = & $Python -c "import secrets; print(secrets.token_urlsafe(24))"
    $Template.Replace("change-me-before-first-run", $Token) | Set-Content $EnvPath -NoNewline
}

$Address = (Get-NetIPAddress -AddressFamily IPv4 | Where-Object {
    $_.IPAddress -notlike "127.*" -and $_.PrefixOrigin -ne "WellKnown"
} | Select-Object -First 1 -ExpandProperty IPAddress)
$ConfiguredToken = (Select-String -Path $EnvPath -Pattern '^MAZ_TOKEN=(.+)$').Matches.Groups[1].Value
Write-Host "MAZ Host is ready."
Write-Host "On MAZ Pocket open Connections, press C, then enter:"
Write-Host "  Address: ${Address}:8787"
Write-Host "  Token:   $ConfiguredToken"
Write-Host "Run .\run.ps1 to start the host."
