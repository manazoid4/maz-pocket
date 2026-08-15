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
    $Token = & $Python -c "import secrets; print(secrets.token_urlsafe(12))"
    $Template.Replace("change-me-before-first-run", $Token) | Set-Content $EnvPath -NoNewline
}

$Address = (Get-NetIPAddress -AddressFamily IPv4 | Where-Object {
    $_.IPAddress -notlike "127.*" -and $_.PrefixOrigin -ne "WellKnown"
} | Select-Object -First 1 -ExpandProperty IPAddress)
$ConfiguredToken = (Select-String -Path $EnvPath -Pattern '^MAZ_TOKEN=(.+)$').Matches.Groups[1].Value
Write-Host "MAZ Core is ready."
Write-Host "On MAZ Pocket use CONTROL > MAZ CORE / legacy host config if pairing is needed:"
Write-Host "  Address: ${Address}:8787"
Write-Host "  Token:   $ConfiguredToken"
Write-Host "Run .\run.ps1 to start Core, or .\install-core.ps1 for the full v0.5 one-shot setup."
Write-Host "With MAZ Pocket open over USB, .\pair.ps1 remains available for serial pairing."
