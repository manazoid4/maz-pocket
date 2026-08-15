param(
    [string]$Drive = "",
    [string]$FirmwarePath = ""
)

$ErrorActionPreference = "Stop"
$Version = "0.5.1"
$FirmwareName = "Maz-Pocket-v$Version-M5Launcher.bin"

function Fail([string]$Message) {
    Write-Host "MAZ Pocket: $Message" -ForegroundColor Red
    exit 1
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $FirmwarePath) {
    $candidate = Join-Path $scriptRoot $FirmwareName
    if (-not (Test-Path $candidate)) {
        $candidate = Join-Path (Split-Path -Parent $scriptRoot) $FirmwareName
    }
    $FirmwarePath = $candidate
}

if (-not (Test-Path $FirmwarePath -PathType Leaf)) {
    Fail "firmware file not found: $FirmwarePath"
}

$bytes = [IO.File]::ReadAllBytes((Resolve-Path $FirmwarePath))
if ($bytes.Length -lt 65536 -or $bytes[0] -ne 0xE9) {
    Fail "the selected file is not a valid ESP32 app image"
}
if ($bytes.Length -gt 0x180000) {
    Fail "firmware is larger than the known M5Launcher slot ceiling"
}

if (-not $Drive) {
    $removable = @(Get-CimInstance Win32_LogicalDisk -Filter "DriveType=2" | Where-Object { $_.DeviceID })
    if ($removable.Count -eq 1) {
        $Drive = $removable[0].DeviceID
    } else {
        Write-Host "Detected removable drives:" -ForegroundColor Cyan
        if ($removable.Count -eq 0) {
            Write-Host "  none detected automatically"
        } else {
            $removable | ForEach-Object { Write-Host "  $($_.DeviceID)  $($_.VolumeName)  $([math]::Round($_.FreeSpace / 1MB)) MB free" }
        }
        $Drive = Read-Host "Enter the microSD drive letter (example E:)"
    }
}

$Drive = $Drive.Trim().TrimEnd([char]92)
if ($Drive -notmatch '^[A-Za-z]:$') {
    Fail "drive must look like E:"
}

$root = "$Drive\"
if (-not (Test-Path $root -PathType Container)) {
    Fail "drive $Drive is not mounted"
}

$disk = Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='$Drive'" -ErrorAction SilentlyContinue
if ($disk -and $disk.DriveType -eq 3) {
    Fail "$Drive is a fixed disk. This installer only writes to removable media unless you explicitly copy the .bin yourself."
}

$source = (Resolve-Path $FirmwarePath).Path
$target = Join-Path $root $FirmwareName
$sourceHash = (Get-FileHash $source -Algorithm SHA256).Hash.ToLowerInvariant()

Write-Host ""
Write-Host "MAZ Pocket v$Version -> $target" -ForegroundColor Cyan
Write-Host "This only copies one app image. It will NOT format the card or flash the Cardputer." -ForegroundColor Yellow
$answer = Read-Host "Type COPY to continue"
if ($answer -cne "COPY") {
    Fail "cancelled; nothing was changed"
}

Copy-Item $source $target -Force
$targetHash = (Get-FileHash $target -Algorithm SHA256).Hash.ToLowerInvariant()
if ($sourceHash -ne $targetHash) {
    Remove-Item $target -Force -ErrorAction SilentlyContinue
    Fail "copy verification failed; copied file was removed"
}

$quick = Join-Path $scriptRoot "QUICKSTART.txt"
if (-not (Test-Path $quick)) {
    $quick = Join-Path (Split-Path -Parent $scriptRoot) "QUICKSTART.txt"
}
if (Test-Path $quick) {
    Copy-Item $quick (Join-Path $root "MAZ-Pocket-QUICKSTART.txt") -Force
}

Write-Host ""
Write-Host "Verified copy complete." -ForegroundColor Green
Write-Host "SHA256 $sourceHash"
Write-Host "1. Safely eject the microSD card."
Write-Host "2. Put it in the Cardputer ADV and boot M5Launcher."
Write-Host "3. Select $FirmwareName -> Install -> Launch."
Write-Host "4. Do not flash this app-only image at address 0x0."
