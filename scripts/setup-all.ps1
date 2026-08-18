param(
    [switch]$SkipCore,
    [switch]$SkipSd,
    [switch]$NoBrowser
)

$ErrorActionPreference = "Stop"
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = $Here
$VersionPath = Join-Path $Root "VERSION"
if (-not (Test-Path $VersionPath)) {
    $Root = Split-Path -Parent $Here
    $VersionPath = Join-Path $Root "VERSION"
}
if (-not (Test-Path $VersionPath)) { throw "VERSION file not found beside START-HERE package." }
$Version = (Get-Content $VersionPath -Raw).Trim()
$FirmwareName = "Maz-Pocket-v$Version-M5Launcher.bin"
$CoreName = "MAZ-Core-v$Version.zip"

function Find-PackageFile([string]$Name) {
    $local = Join-Path $Here $Name
    if (Test-Path $local) { return $local }
    $rootFile = Join-Path $Root $Name
    if (Test-Path $rootFile) { return $rootFile }
    return $null
}

function Ask-Yes([string]$Prompt, [bool]$DefaultYes = $true) {
    $Suffix = if ($DefaultYes) { "[Y/n]" } else { "[y/N]" }
    $Choice = Read-Host "$Prompt $Suffix"
    if ([string]::IsNullOrWhiteSpace($Choice)) { return $DefaultYes }
    return $Choice -match '^[Yy]'
}

Write-Host ""
Write-Host "MAZ POCKET v$Version - START HERE" -ForegroundColor Cyan
Write-Host "You choose each part. Nothing formats a drive or writes Cardputer flash directly." -ForegroundColor DarkGray
Write-Host ""
Write-Host "OPTION 1 - MAZ Core: PC companion for Call MAZ, agents, phone approvals and projects."
Write-Host "OPTION 2 - Cardputer file: copy the firmware .bin to a microSD for M5Launcher."
Write-Host "OPTION 3 - Portal: open mazpocket.local after setup."
Write-Host ""

$CoreInstalled = $false
$SdCopied = $false

if (-not $SkipCore) {
    if (Ask-Yes "Install or update MAZ Core on this PC?" $true) {
        $CoreZip = Find-PackageFile $CoreName
        if ($CoreZip) {
            $CoreHome = Join-Path $env:LOCALAPPDATA "MAZ Core"
            $Temp = Join-Path $env:TEMP ("maz-core-" + [guid]::NewGuid().ToString("N"))
            New-Item -ItemType Directory -Force $Temp | Out-Null
            try {
                $SavedEnv = $null
                $ExistingEnv = Join-Path $CoreHome ".env"
                if (Test-Path $ExistingEnv) { $SavedEnv = Get-Content $ExistingEnv -Raw }

                Expand-Archive -Path $CoreZip -DestinationPath $Temp -Force
                New-Item -ItemType Directory -Force $CoreHome | Out-Null
                Copy-Item (Join-Path $Temp "*") $CoreHome -Recurse -Force
                if ($null -ne $SavedEnv) { Set-Content -Path $ExistingEnv -Value $SavedEnv -NoNewline -Encoding utf8 }

                Write-Host "Installing MAZ Core into $CoreHome" -ForegroundColor Cyan
                & (Join-Path $CoreHome "install-core.ps1") -NoBrowser
                $CoreInstalled = $true
            } finally {
                Remove-Item $Temp -Recurse -Force -ErrorAction SilentlyContinue
            }
        } else {
            $DevCore = Join-Path $Root "host\install-core.ps1"
            if (Test-Path $DevCore) {
                Write-Host "Using repository MAZ Core in place." -ForegroundColor Yellow
                & $DevCore -NoBrowser
                $CoreInstalled = $true
            } else {
                Write-Warning "$CoreName not found; skipping Core install."
            }
        }
    } else {
        Write-Host "MAZ Core skipped by choice." -ForegroundColor Yellow
    }
}

if (-not $SkipSd) {
    $Firmware = Find-PackageFile $FirmwareName
    $SdInstaller = Find-PackageFile "install-to-sd.ps1"
    if (-not $SdInstaller) { $SdInstaller = Join-Path $Root "scripts\install-to-sd.ps1" }
    $Removable = @(Get-CimInstance Win32_LogicalDisk -Filter "DriveType=2" | Where-Object { $_.DeviceID })

    if ($Firmware -and (Test-Path $SdInstaller) -and $Removable.Count -eq 1) {
        $Disk = $Removable[0]
        Write-Host ""
        Write-Host "One removable drive detected: $($Disk.DeviceID) $($Disk.VolumeName)" -ForegroundColor Cyan
        if (Ask-Yes "Copy MAZ Pocket v$Version firmware there now?" $true) {
            & $SdInstaller -Drive $Disk.DeviceID -FirmwarePath $Firmware -Yes
            $SdCopied = $true
        } else {
            Write-Host "SD copy skipped by choice."
        }
    } elseif ($Removable.Count -eq 0) {
        Write-Host ""
        Write-Host "No removable microSD detected - that is fine." -ForegroundColor Yellow
        Write-Host "If MAZ Pocket is already running, stage the new .bin later at http://mazpocket.local."
    } elseif ($Removable.Count -gt 1) {
        Write-Host ""
        Write-Warning "Multiple removable drives detected, so setup will not guess which one is the Cardputer microSD."
        Write-Host "Run INSTALL-MAZ-POCKET.cmd when you want to choose the drive explicitly."
    } elseif (-not $Firmware) {
        Write-Warning "$FirmwareName not found; firmware copy skipped."
    }
}

Write-Host ""
Write-Host "SETUP COMPLETE" -ForegroundColor Green
Write-Host "MAZ Core: $(if ($CoreInstalled) { 'installed/updated' } elseif ($SkipCore) { 'skipped by command' } else { 'not installed' })"
Write-Host "Cardputer SD file: $(if ($SdCopied) { 'copied' } elseif ($SkipSd) { 'skipped by command' } else { 'not copied' })"
Write-Host "Core location: $env:LOCALAPPDATA\MAZ Core"
Write-Host "Cardputer portal: http://mazpocket.local"
Write-Host "Update path: choose .bin at mazpocket.local -> Verify/Stage -> Open M5Launcher -> Install -> Launch MAZ Pocket."
Write-Host "Returning to M5Launcher no longer intentionally invalidates the installed MAZ Pocket image."

if (-not $NoBrowser) {
    Write-Host ""
    if (Ask-Yes "Open mazpocket.local now?" $true) {
        try { Start-Process "http://mazpocket.local" } catch { }
    } else {
        Write-Host "Portal not opened. Use http://mazpocket.local whenever you want it."
    }
}
