$ErrorActionPreference = "Stop"
$HostRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$EnvPath = Join-Path $HostRoot ".env"

Write-Host "MAZ Core v0.5 - installing..."
& (Join-Path $HostRoot "setup.ps1")

function Set-MazEnv([string]$Name, [string]$Value) {
    $lines = @(Get-Content $EnvPath)
    $prefix = "$Name="
    $found = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i].StartsWith($prefix)) {
            $lines[$i] = "$prefix$Value"
            $found = $true
        }
    }
    if (-not $found) { $lines += "$prefix$Value" }
    $lines | Set-Content -Encoding utf8 $EnvPath
}

$Desktop = Join-Path $env:USERPROFILE "Desktop"
$Projects = Join-Path $env:USERPROFILE "Projects"
$Roots = @($Desktop)
if (Test-Path $Projects) { $Roots += $Projects }
Set-MazEnv "MAZ_CORE_ENABLED" "true"
Set-MazEnv "MAZ_PROJECT_ROOTS" ($Roots -join ";")

$Obsidian = Join-Path $Desktop "Obsidian Main Vault"
if (Test-Path $Obsidian) { Set-MazEnv "MAZ_OBSIDIAN_ROOT" $Obsidian }
Set-MazEnv "MAZ_OLLAMA_MODEL" "lfm2.5-8b-a1b-gpu:latest"
Set-MazEnv "MAZ_DEFAULT_ROUTE" "local"
Set-MazEnv "MAZ_CARDPUTER_URL" "http://mazpocket.local"
Set-MazEnv "MAZ_WEB_ORIGINS" "https://mazos-site.vercel.app,http://localhost:3000,http://127.0.0.1:3000"
Set-MazEnv "MAZ_BRIDGE_REPO" "manazoid4/maz-pocket"

$Bridge = $false
$Gh = Get-Command gh -ErrorAction SilentlyContinue
if ($Gh) {
    & gh auth status *> $null
    if ($LASTEXITCODE -eq 0) { $Bridge = $true }
}
Set-MazEnv "MAZ_BRIDGE_ENABLED" ($(if ($Bridge) { "true" } else { "false" }))

# Keep MAZ Core alive after sign-in without installing a privileged service.
$Startup = [Environment]::GetFolderPath("Startup")
$ShortcutPath = Join-Path $Startup "MAZ Core.lnk"
$Shell = New-Object -ComObject WScript.Shell
$Shortcut = $Shell.CreateShortcut($ShortcutPath)
$Shortcut.TargetPath = "powershell.exe"
$Shortcut.Arguments = "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$(Join-Path $HostRoot 'run.ps1')`""
$Shortcut.WorkingDirectory = $HostRoot
$Shortcut.Description = "MAZ Core v0.5"
$Shortcut.Save()

# Open the LAN port automatically when the script is elevated. Otherwise the
# normal Python/Windows firewall prompt can be accepted once.
$Admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if ($Admin) {
    $existing = Get-NetFirewallRule -DisplayName "MAZ Core 8787" -ErrorAction SilentlyContinue
    if (-not $existing) {
        New-NetFirewallRule -DisplayName "MAZ Core 8787" -Direction Inbound -Action Allow -Protocol TCP -LocalPort 8787 -Profile Private | Out-Null
    }
}

# Start Core now if it is not already listening.
$Listening = Get-NetTCPConnection -LocalPort 8787 -State Listen -ErrorAction SilentlyContinue
if (-not $Listening) {
    Start-Process powershell.exe -WindowStyle Hidden -ArgumentList "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$(Join-Path $HostRoot 'run.ps1')`""
    Start-Sleep -Seconds 2
}

# Optional private HTTPS endpoint for the hidden Maz Works console. Tailscale
# Serve stays tailnet-private; this script never writes the URL into the public site.
$Tail = Get-Command tailscale -ErrorAction SilentlyContinue
if ($Tail) {
    try {
        & tailscale serve --bg --yes 8787 *> $null
    } catch { }
}

$Address = (Get-NetIPAddress -AddressFamily IPv4 | Where-Object {
    $_.IPAddress -notlike "127.*" -and $_.PrefixOrigin -ne "WellKnown"
} | Select-Object -First 1 -ExpandProperty IPAddress)
$Token = (Select-String -Path $EnvPath -Pattern '^MAZ_TOKEN=(.+)$').Matches.Groups[1].Value

Write-Host ""
Write-Host "MAZ Core v0.5 READY"
Write-Host "PC address: ${Address}:8787"
Write-Host "Pair token: $Token"
Write-Host "GitHub bridge: $(if ($Bridge) { 'ON' } else { 'OFF - sign into gh if you want ChatGPT -> PC commands' })"
Write-Host "Cardputer: CONTROL > MAZ CORE, enter the address/token if not already paired."
Write-Host "Hidden Maz Works console: enter your private Core/Tailscale URL there; it is never stored in the site source."
