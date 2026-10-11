<#
.SYNOPSIS
  Recover a Cardputer ADV over USB: erase the whole flash, then write a full nod image at 0x0.

.DESCRIPTION
  WARNING: this wipes EVERYTHING on the device, including M5 Launcher. Use it only for
  recovery. The normal route is the app-only .bin through M5 Launcher
  (see docs/RECOVER-FROM-ERASED.md).

  Image: -Image <file>, else nod-factory-8MB.bin next to this script, else it is built from
  .pio\build\cardputer-adv (run `pio run` first) with esptool merge_bin.
  Uses esptool from PlatformIO's own packages (no global install). Prints PASS or FAIL.
  -DryRun prints what it would run and touches nothing.
#>
param(
    [string]$Image = "",
    [string]$Port = "",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$PioHome = Join-Path $env:USERPROFILE ".platformio"

function Fail($msg) { Write-Host ""; Write-Host "FAIL: $msg" -ForegroundColor Red; exit 1 }

# --- python + esptool (PlatformIO's own copies) ---
$PyCandidates = @((Join-Path $PioHome "penv\Scripts\python.exe"))
$OnPath = Get-Command python.exe -ErrorAction SilentlyContinue | Select-Object -First 1
if ($OnPath) { $PyCandidates += $OnPath.Source }
$Python = $PyCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
$Esptool = Join-Path $PioHome "packages\tool-esptoolpy\esptool.py"
if (-not $Python)              { Fail "Python not found. Install PlatformIO (it brings Python) or Python 3." }
if (-not (Test-Path $Esptool)) { Fail "esptool not found at $Esptool. Install PlatformIO and run 'pio run' once." }

# --- image ---
if (-not $Image) {
    $Beside = Join-Path $Here "nod-factory-8MB.bin"
    if (Test-Path $Beside) { $Image = $Beside }
    else {
        $Root  = Split-Path -Parent $Here
        $Build = Join-Path $Root ".pio\build\cardputer-adv"
        $Boot0 = Join-Path $PioHome "packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin"
        foreach ($f in "bootloader.bin", "partitions.bin", "firmware.bin") {
            if (-not (Test-Path (Join-Path $Build $f))) { Fail "No image given and $Build\$f is missing. Run 'pio run' first." }
        }
        if (-not (Test-Path $Boot0)) { Fail "boot_app0.bin missing at $Boot0." }
        $Image = Join-Path $Root "dist\nod-factory-8MB.bin"
        if ($DryRun) {
            Write-Host "DRY RUN (nothing is written)"
            Write-Host "Would merge bootloader.bin, partitions.bin, boot_app0.bin and firmware.bin into $Image."
            Write-Host "Would validate the factory layout before any flash. No image was generated or device contacted."
            Write-Host "Then: esptool.py --chip esp32s3 write_flash --erase-all 0x0 $Image"
            exit 0
        }
        New-Item -ItemType Directory -Force (Split-Path $Image) | Out-Null
        # Header flags stay as PlatformIO built them (DIO header, 80 MHz, 8 MB); the bootloader switches to QIO itself.
        & $Python $Esptool --chip esp32s3 merge_bin -o $Image --flash_mode keep --flash_freq keep --flash_size keep `
            0x0 (Join-Path $Build "bootloader.bin") 0x8000 (Join-Path $Build "partitions.bin") `
            0xe000 $Boot0 0x10000 (Join-Path $Build "firmware.bin")
        if ($LASTEXITCODE -ne 0) { Fail "Could not build the factory image." }
    }
}
if (-not (Test-Path $Image)) { Fail "Image not found: $Image" }
$Image = (Resolve-Path $Image).Path
$Bytes = [System.IO.File]::ReadAllBytes($Image)
if ($Bytes.Length -lt 0x20000 -or $Bytes.Length -gt 0x800000 -or
    $Bytes[0] -ne 0xE9 -or $Bytes[12] -ne 9 -or $Bytes[13] -ne 0 -or
    $Bytes[0x8000] -ne 0xAA -or $Bytes[0x8001] -ne 0x50 -or
    $Bytes[0x10000] -ne 0xE9 -or $Bytes[0x1000C] -ne 9 -or $Bytes[0x1000D] -ne 0) {
    Fail "$Image is not a Cardputer ESP32-S3 factory image: requires bootloader at 0x0, partition table at 0x8000 and app at 0x10000. App-only firmware.bin must go through M5 Launcher, never at 0x0."
}
$HasAppPartition = $false
for ($Offset = 0x8000; $Offset -lt 0x8C00; $Offset += 32) {
    if ($Bytes[$Offset] -ne 0xAA -or $Bytes[$Offset + 1] -ne 0x50) { break }
    if ($Bytes[$Offset + 2] -eq 0 -and [BitConverter]::ToUInt32($Bytes, $Offset + 4) -eq 0x10000) {
        $HasAppPartition = $true
    }
}
if (-not $HasAppPartition) { Fail "Factory partition table has no app at 0x10000. Nothing was flashed." }

# --- find the port: ESP32-S3 native USB is VID 303A ---
function Find-Port {
    $hit = Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
        Where-Object { $_.PNPDeviceID -match 'VID_303A' -and $_.Name -match '\(COM\d+\)' } |
        Select-Object -First 1
    if ($hit) { return [regex]::Match($hit.Name, 'COM\d+').Value }
    return ""
}

$Common = @("--chip", "esp32s3", "--baud", "460800", "--no-stub")
$Tail   = @("--flash_mode", "keep", "--flash_freq", "keep", "--flash_size", "keep", "0x0", $Image)
$FlashArgs = $Common + @("write_flash", "--erase-all") + $Tail

if ($DryRun) {
    $Found = $Port; if (-not $Found) { $Found = Find-Port }
    if (-not $Found) { $Found = "(none found right now)" }
    Write-Host "DRY RUN (nothing is written)"
    Write-Host "python : $Python"
    Write-Host "esptool: $Esptool"
    Write-Host "image  : $Image ($($Bytes.Length) bytes)"
    Write-Host "port   : $Found"
    Write-Host "command: python esptool.py [--port <port>] $($FlashArgs -join ' ')"
    exit 0
}

if (-not $Port) {
    for ($i = 0; $i -lt 15 -and -not $Port; $i++) {
        $Port = Find-Port
        if (-not $Port) {
            if ($i -eq 0) {
                Write-Host "No Cardputer found. Plug it in with a DATA USB cable."
                Write-Host "If it still does not show up: unplug, HOLD the side button G0, plug in while holding, then let go."
            }
            Start-Sleep -Seconds 1
        }
    }
}
if ($Port) { Write-Host "Using port $Port" }
else       { Fail "No ESP32 port identified. Connect the Cardputer using G0 recovery mode, or specify its verified -Port. Nothing was flashed." }

$PortArgs = @(); if ($Port) { $PortArgs = @("--port", $Port) }
$ErrorActionPreference = "Continue"   # esptool prints progress on stderr; do not treat it as an error

function Run-Esptool($ArgList) {
    $out = & $Python $Esptool @PortArgs @ArgList 2>&1 | ForEach-Object { $s = "$_"; Write-Host $s; $s }
    return @{ Code = $LASTEXITCODE; Text = ($out -join "`n") }
}

Write-Host "Erasing everything and writing the image. Do not unplug."
$R = Run-Esptool $FlashArgs
$Erased = $true
if ($R.Code -ne 0 -and $R.Text -notmatch "Could not connect|Failed to connect|No serial data|could not open port|Could not open|No such port|Found 0 serial ports") {
    # unconfirmed: the S3 ROM loader may refuse the whole-chip erase. Retry without it (still writes + verifies everything).
    Write-Host ""
    Write-Host "Whole-chip erase did not work; retrying with a normal write (old settings may survive)."
    $R = Run-Esptool ($Common + @("write_flash") + $Tail)
    $Erased = $false
}

if ($R.Code -eq 0 -and $R.Text -match "Hash of data verified") {
    Write-Host ""
    $note = ""; if (-not $Erased) { $note = " (no full erase: ROM refused it)" }
    Write-Host "PASS: image written, MD5 check matched$note. The Cardputer is restarting into nod." -ForegroundColor Green
    exit 0
}
if ($R.Text -match "Could not connect|Failed to connect|No serial data|could not open port|Could not open|No such port|Found 0 serial ports") {
    Fail "Could not talk to the device (nothing was written). Unplug, HOLD G0, plug in while holding, let go, run this again."
}
Fail "esptool exit code $($R.Code) and no 'Hash of data verified' line. The device may be half-written: run this again."
