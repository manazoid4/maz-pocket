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

Write-Host ""
Write-Host "MAZ POCKET v$Version - START HERE" -ForegroundColor Cyan
Write-Host "This setup never formats a drive and never writes Cardputer flash directly." -ForegroundColor DarkGray
Write-Host ""

if (-not $SkipCore) {
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
        } finally {
            Remove-Item $Temp -Recurse -Force -ErrorAction SilentlyContinue
        }
    } else {
        $DevCore = Join-Path $Root "host\install-core.ps1"
        if (Test-Path $DevCore) {
            Write-Host "Using repository MAZ Core in place." -ForegroundColor Yellow
            & $DevCore -NoBrowser
        } else {
            Write-Warning "$CoreName not found; skipping Core install."
        }
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
        $Choice = Read-Host "Copy MAZ Pocket v$Version firmware there now? [Y/n]"
        if ([string]::IsNullOrWhiteSpace($Choice) -or $Choice -match '^[Yy]') {
            & $SdInstaller -Drive $Disk.DeviceID -FirmwarePath $Firmware -Yes
        } else {
            Write-Host "SD copy skipped."
        }
    } elseif ($Removable.Count -eq 0) {
        Write-Host ""
        Write-Host "No removable microSD detected - that is fine." -ForegroundColor Yellow
        Write-Host "If your Cardputer already runs v0.5.2+, download the v$Version .bin on your phone and stage it at http://mazpocket.local."
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
Write-Host "Core lives at: $env:LOCALAPPDATA\MAZ Core"
Write-Host "Cardputer portal: http://mazpocket.local"
Write-Host "Normal future update: phone .bin -> mazpocket.local -> Verify/Stage -> M5Launcher -> Install/Launch."
Write-Host "Fresh firmware install: copy the .bin to microSD -> M5Launcher -> Install/Launch."

if (-not $NoBrowser) {
    try { Start-Process "http://mazpocket.local" } catch { }
}
