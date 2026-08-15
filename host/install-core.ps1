param(
    [switch]$NoBrowser
)

$ErrorActionPreference = "Stop"
$HostRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$EnvPath = Join-Path $HostRoot ".env"
$VersionPath = Join-Path $HostRoot "VERSION"
if (-not (Test-Path $VersionPath)) { $VersionPath = Join-Path (Split-Path -Parent $HostRoot) "VERSION" }
$Version = if (Test-Path $VersionPath) { (Get-Content $VersionPath -Raw).Trim() } else { "dev" }

Write-Host "MAZ Core v$Version - installing..."
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

function Get-MazEnv([string]$Name) {
    $escaped = [regex]::Escape($Name)
    $match = Select-String -Path $EnvPath -Pattern "^$escaped=(.*)$" | Select-Object -First 1
    if ($match -and $match.Matches.Count) { return $match.Matches[0].Groups[1].Value.Trim() }
    return ""
}

$Desktop = Join-Path $env:USERPROFILE "Desktop"
$Projects = Join-Path $env:USERPROFILE "Projects"
$Roots = @($Desktop)
if (Test-Path $Projects) { $Roots += $Projects }
Set-MazEnv "MAZ_CORE_ENABLED" "true"
Set-MazEnv "MAZ_PROJECT_ROOTS" ($Roots -join ";")

$Obsidian = Join-Path $Desktop "Obsidian Main Vault"
if (Test-Path $Obsidian) { Set-MazEnv "MAZ_OBSIDIAN_ROOT" $Obsidian }
Set-MazEnv "MAZ_DEFAULT_ROUTE" "local"
Set-MazEnv "MAZ_CARDPUTER_URL" "http://mazpocket.local"
Set-MazEnv "MAZ_WEB_ORIGINS" "https://mazos-site.vercel.app,http://localhost:3000,http://127.0.0.1:3000"
Set-MazEnv "MAZ_BRIDGE_REPO" "manazoid4/maz-pocket"
Set-MazEnv "MAZ_LOCAL_MODEL_POLICY" "auto"

# Preserve working model choices. If the configured primary disappeared, pick
# a real installed model. Then choose a distinct installed backup when possible.
$CurrentPrimary = Get-MazEnv "MAZ_OLLAMA_MODEL"
$CurrentBackup = Get-MazEnv "MAZ_OLLAMA_BACKUP_MODEL"
$SelectedPrimary = $CurrentPrimary
$SelectedBackup = $CurrentBackup
try {
    $Tags = Invoke-RestMethod -Uri "http://127.0.0.1:11434/api/tags" -Method Get -TimeoutSec 2
    $Installed = @($Tags.models | ForEach-Object { $_.name } | Where-Object { $_ })
    if ($Installed.Count -gt 0) {
        $Preferred = @(
            "qwen3.5:4b",
            "lfm2.5-8b-a1b-gpu:latest",
            "qwen3:4b-q4_K_M",
            "qwen3:4b",
            "gemma3:4b",
            "llama3.1:8b"
        )
        if (-not $SelectedPrimary -or $Installed -notcontains $SelectedPrimary) {
            $SelectedPrimary = $Preferred | Where-Object { $Installed -contains $_ } | Select-Object -First 1
            if (-not $SelectedPrimary) { $SelectedPrimary = $Installed[0] }
        }
        if (-not $SelectedBackup -or $Installed -notcontains $SelectedBackup -or $SelectedBackup -eq $SelectedPrimary) {
            $SelectedBackup = $Preferred | Where-Object { $Installed -contains $_ -and $_ -ne $SelectedPrimary } | Select-Object -First 1
            if (-not $SelectedBackup) {
                $SelectedBackup = $Installed | Where-Object { $_ -ne $SelectedPrimary } | Select-Object -First 1
            }
        }
        if (-not $SelectedBackup) { $SelectedBackup = "" }
        Set-MazEnv "MAZ_OLLAMA_MODEL" $SelectedPrimary
        Set-MazEnv "MAZ_OLLAMA_BACKUP_MODEL" $SelectedBackup
        Write-Host "Local AI primary: $SelectedPrimary"
        Write-Host "Local AI backup:  $(if ($SelectedBackup) { $SelectedBackup } else { 'none installed' })"
    } else {
        Write-Warning "Ollama is reachable but no local models are installed; preserving configured model names."
    }
} catch {
    Write-Warning "Ollama is not reachable yet; preserving configured model names."
}

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
$Shortcut.Description = "MAZ Core v$Version"
$Shortcut.Save()

# Open the LAN port automatically when elevated. Otherwise Windows may ask once.
$Admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if ($Admin) {
    $existing = Get-NetFirewallRule -DisplayName "MAZ Core 8787" -ErrorAction SilentlyContinue
    if (-not $existing) {
        New-NetFirewallRule -DisplayName "MAZ Core 8787" -Direction Inbound -Action Allow -Protocol TCP -LocalPort 8787 -Profile Private | Out-Null
    }
}

$Listening = Get-NetTCPConnection -LocalPort 8787 -State Listen -ErrorAction SilentlyContinue
if (-not $Listening) {
    Start-Process powershell.exe -WindowStyle Hidden -ArgumentList "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$(Join-Path $HostRoot 'run.ps1')`""
    Start-Sleep -Seconds 2
}

# Optional private HTTPS endpoint for the Maz Works capability clients.
$Tail = Get-Command tailscale -ErrorAction SilentlyContinue
if ($Tail) {
    try { & tailscale serve --bg --yes 8787 *> $null } catch { }
}

$Address = Get-NetIPAddress -AddressFamily IPv4 | Where-Object {
    $_.InterfaceAlias -eq "Wi-Fi" -and $_.IPAddress -notlike "127.*" -and $_.IPAddress -notlike "169.254.*"
} | Select-Object -First 1 -ExpandProperty IPAddress
if (-not $Address) {
    $Address = Get-NetIPAddress -AddressFamily IPv4 | Where-Object {
        $_.IPAddress -notlike "127.*" -and $_.IPAddress -notlike "169.254.*" -and $_.PrefixOrigin -ne "WellKnown"
    } | Select-Object -First 1 -ExpandProperty IPAddress
}
$Token = Get-MazEnv "MAZ_TOKEN"

# If a v0.6 Cardputer is connected by USB, pair Core without asking for Wi-Fi
# credentials or touching the device's known-good network configuration.
$UsbPaired = $false
$PortName = Get-CimInstance Win32_SerialPort | Where-Object { $_.PNPDeviceID -match 'VID_303A&PID_1001' } | Select-Object -First 1 -ExpandProperty DeviceID
if ($PortName) {
    try {
        & (Join-Path $HostRoot "pair.ps1") -Quiet
        $UsbPaired = $true
    } catch {
        Write-Warning "USB Cardputer found but Core-only pairing was not completed: $($_.Exception.Message)"
    }
}

# A durable shortcut makes the device portal one click away.
try {
    $PortalShortcut = Join-Path $Desktop "MAZ Pocket.url"
    @("[InternetShortcut]", "URL=http://mazpocket.local") | Set-Content -Encoding ascii $PortalShortcut
} catch { }

Write-Host ""
Write-Host "MAZ Core v$Version READY" -ForegroundColor Green
Write-Host "PC address: ${Address}:8787"
Write-Host "Pair token: $Token"
Write-Host "Local model chain: $(Get-MazEnv 'MAZ_OLLAMA_MODEL') -> $(Get-MazEnv 'MAZ_OLLAMA_BACKUP_MODEL')"
Write-Host "USB pairing: $(if ($UsbPaired) { 'DONE' } else { 'not required / not available' })"
Write-Host "GitHub bridge: $(if ($Bridge) { 'ON' } else { 'OFF - sign into gh if you want AI -> private GitHub -> PC actions' })"
Write-Host "Cardputer portal: http://mazpocket.local"
Write-Host "Maz Works: /maz-core and /maz-pocket-ai"

if (-not $NoBrowser) {
    try { Start-Process "http://mazpocket.local" } catch { }
}
