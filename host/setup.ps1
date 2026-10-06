$ErrorActionPreference = "Stop"
$HostRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Python = Join-Path $HostRoot ".venv\Scripts\python.exe"
$VersionPath = Join-Path $HostRoot "VERSION"
if (-not (Test-Path $VersionPath)) { $VersionPath = Join-Path (Split-Path -Parent $HostRoot) "VERSION" }
$Version = if (Test-Path $VersionPath) { (Get-Content $VersionPath -Raw).Trim() } else { "dev" }

if (-not (Test-Path $Python)) {
    $Py = Get-Command py -ErrorAction SilentlyContinue
    $PythonCmd = Get-Command python -ErrorAction SilentlyContinue
    if ($Py) {
        & py -3 -m venv (Join-Path $HostRoot ".venv")
    } elseif ($PythonCmd) {
        & python -m venv (Join-Path $HostRoot ".venv")
    } else {
        throw "Python 3 is required for MAZ Core. Install Python 3.11+ once, then double-click START-HERE.cmd again."
    }
}
& $Python -m pip install --disable-pip-version-check -r (Join-Path $HostRoot "requirements.txt")

$EnvPath = Join-Path $HostRoot ".env"
if (-not (Test-Path $EnvPath)) {
    $Template = Get-Content (Join-Path $HostRoot ".env.example") -Raw
    $Token = & $Python -c "import secrets; print(secrets.token_urlsafe(12))"
    $Template.Replace("change-me-before-first-run", $Token) | Set-Content $EnvPath -NoNewline
}

$Address = Get-NetIPAddress -AddressFamily IPv4 | Where-Object {
    $_.InterfaceAlias -eq "Wi-Fi" -and $_.IPAddress -notlike "127.*" -and $_.IPAddress -notlike "169.254.*"
} | Select-Object -First 1 -ExpandProperty IPAddress
if (-not $Address) {
    $Address = Get-NetIPAddress -AddressFamily IPv4 | Where-Object {
        $_.IPAddress -notlike "127.*" -and $_.IPAddress -notlike "169.254.*" -and $_.PrefixOrigin -ne "WellKnown"
    } | Select-Object -First 1 -ExpandProperty IPAddress
}
$ConfiguredToken = (Select-String -Path $EnvPath -Pattern '^MAZ_TOKEN=(.+)$').Matches.Groups[1].Value
$Sha = [Security.Cryptography.SHA256]::Create()
try {
    $Digest = $Sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($ConfiguredToken))
    $TokenId = -join ($Digest[0..5] | ForEach-Object { $_.ToString("x2") })
} finally {
    $Sha.Dispose()
}
Write-Host "MAZ Core v$Version base setup is ready."
Write-Host "Cardputer pairing if needed:"
Write-Host "  Address: ${Address}:8787"
Write-Host "  Token ID: $TokenId (secret remains in .env and on the paired Cardputer)"
Write-Host "Run install-core.ps1 for the full setup. With v0.6 open over USB, pair.ps1 updates Core details without asking for Wi-Fi credentials."
