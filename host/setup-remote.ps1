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

# Tell Core its own remote URL (Core hands it to the device over /health; zero typing on the device).
$envFile = Join-Path $env:LOCALAPPDATA "MAZ Core\.env"
try {
    $dir = Split-Path $envFile
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    $lines = @()
    if (Test-Path $envFile) { $lines = @(Get-Content -LiteralPath $envFile | Where-Object { $_ -notmatch '^\s*MAZ_REMOTE_URL\s*=' }) }
    $lines += "MAZ_REMOTE_URL=$url"
    Set-Content -LiteralPath $envFile -Value $lines -Encoding ASCII
    Write-Host "Saved MAZ_REMOTE_URL to $envFile"
} catch {
    Write-Host "Could not update $envFile : $($_.Exception.Message)" -ForegroundColor Yellow
}

# Tailnet IPv4 for the phone (direct, no Funnel).
$tip = ""
try { $tip = (& $ts ip -4 | Select-Object -First 1) } catch { }

# Windows Firewall: allow TCP $Port only from the Tailscale range (100.64.0.0/10). Needs admin.
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
$ruleName = "nod Core (Tailscale)"
if ($isAdmin) {
    try {
        Get-NetFirewallRule -DisplayName $ruleName -ErrorAction SilentlyContinue | Remove-NetFirewallRule
        New-NetFirewallRule -DisplayName $ruleName -Direction Inbound -Action Allow -Protocol TCP -LocalPort $Port -RemoteAddress "100.64.0.0/10" -Profile Any | Out-Null
        Write-Host "Firewall rule '$ruleName' set (TCP $Port from 100.64.0.0/10 only)."
    } catch {
        Write-Host "Firewall rule failed: $($_.Exception.Message)" -ForegroundColor Yellow
    }
} else {
    Write-Host "Not elevated: skipped the firewall rule. Re-run this script as Administrator so your phone can reach Core over the tailnet." -ForegroundColor Yellow
}

# Restart Core so it picks up the new setting.
try {
    if (Get-ScheduledTask -TaskName "nod Core" -ErrorAction SilentlyContinue) {
        Stop-ScheduledTask -TaskName "nod Core" -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 2
        Start-ScheduledTask -TaskName "nod Core"
        Write-Host "Restarted the 'nod Core' task."
    } else {
        Write-Host "Task 'nod Core' not found; restart Core yourself." -ForegroundColor Yellow
    }
} catch {
    Write-Host "Could not restart 'nod Core': $($_.Exception.Message)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Public URL (HTTPS, token still required):" -ForegroundColor Green
Write-Host "    $url"
Write-Host "    Check: $url/health (no token needed, returns only ok/name/version)"
if ($tip) {
    Write-Host "Phone over Tailscale (Tailscale app on, no Funnel needed):" -ForegroundColor Green
    Write-Host "    http://${tip}:$Port/control"
}
Write-Host ""
Write-Host "The Cardputer learns the public URL from Core automatically the next time it reaches Core on your home Wi-Fi. Nothing to type."
Write-Host "Phone approvals: open $url/control/ and sign in with your MAZ token (once)."
Write-Host ""
Write-Host "REMINDER: in the Tailscale admin console (Machines), choose 'Disable key expiry' for this PC," -ForegroundColor Yellow
Write-Host "          otherwise remote access silently stops when the node key expires."
Write-Host "Stop publishing any time with:  host\setup-remote.ps1 -Off"
