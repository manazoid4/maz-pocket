# setup-remote.ps1 - publish nod Core to the internet over Tailscale Funnel (HTTPS).
# ASCII only. Never prints or stores MAZ_TOKEN. Safe to re-run.
#
#   powershell -ExecutionPolicy Bypass -File host\setup-remote.ps1
#   powershell -ExecutionPolicy Bypass -File host\setup-remote.ps1 -Off     # stop publishing
param(
    [int]$Port = 8787,
    [switch]$Off
)

$ErrorActionPreference = "Stop"

function Find-Tailscale {
    $cmd = Get-Command tailscale -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    foreach ($p in @("$env:ProgramFiles\Tailscale\tailscale.exe", "${env:ProgramFiles(x86)}\Tailscale\tailscale.exe")) {
        if (Test-Path $p) { return $p }
    }
    return $null
}

$ts = Find-Tailscale
if (-not $ts) {
    Write-Host "Tailscale is not installed." -ForegroundColor Yellow
    Write-Host "Install it, sign in, then run this script again:"
    Write-Host "    winget install --id Tailscale.Tailscale -e"
    exit 1
}

if ($Off) {
    & $ts funnel reset
    Write-Host "Funnel stopped. Core is no longer reachable from the internet."
    exit 0
}

# Logged in? (BackendState must be Running)
$state = ""
try { $state = (& $ts status --json | ConvertFrom-Json).BackendState } catch { }
if ($state -ne "Running") {
    Write-Host "Tailscale is not signed in / running (state: '$state')." -ForegroundColor Yellow
    Write-Host "Open the Tailscale app, sign in, then run this script again."
    Write-Host "(or: tailscale up)"
    exit 1
}

# Is Core listening locally?
$listening = $false
try { $listening = (Test-NetConnection -ComputerName 127.0.0.1 -Port $Port -WarningAction SilentlyContinue).TcpTestSucceeded } catch { }
if (-not $listening) {
    Write-Host "Warning: nothing is listening on 127.0.0.1:$Port. Start nod Core first (host\run.ps1)." -ForegroundColor Yellow
}

# Publish http://127.0.0.1:<port> at https://<machine>.<tailnet>.ts.net (port 443), persistent across reboots.
# First run: if Funnel / HTTPS certificates are not yet enabled for your tailnet, the CLI prints an
# admin-console link. Open it, click enable, and re-run this script.
Write-Host "Running: tailscale funnel --bg $Port"
& $ts funnel --bg $Port
if ($LASTEXITCODE -ne 0) {
    Write-Host "tailscale funnel failed. If it printed a link, open it to enable Funnel/HTTPS, then re-run." -ForegroundColor Red
    exit $LASTEXITCODE
}

$dns = ""
try { $dns = ((& $ts status --json | ConvertFrom-Json).Self.DNSName).TrimEnd('.') } catch { }
if (-not $dns) {
    Write-Host "Could not read the machine DNS name. Run 'tailscale funnel status' to see your URL."
    exit 0
}
$url = "https://$dns"

Write-Host ""
Write-Host "Remote URL (public HTTPS, token still required):" -ForegroundColor Green
Write-Host "    $url"
Write-Host ""
Write-Host "Check from your phone browser (no token needed, returns only ok/name/version):"
Write-Host "    $url/health"
Write-Host ""
Write-Host "Enter it on the Cardputer (needs to be on your home Wi-Fi once, or use its setup AP):"
Write-Host "    Open the device web page -> unlock -> 'Remote URL' field -> paste the URL above -> Save."
Write-Host "    The device tries your home LAN first and falls back to this URL."
Write-Host ""
Write-Host "Phone approvals: open $url/control/ and sign in with your MAZ token (once)."
Write-Host "Stop publishing any time with:  host\setup-remote.ps1 -Off"
