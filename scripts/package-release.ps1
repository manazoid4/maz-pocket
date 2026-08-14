$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Push-Location $Root
try {
    # Use the active interpreter, not the Windows `py` launcher. On GitHub
    # Actions `py` can select a different preinstalled Python than setup-python,
    # which makes PlatformIO appear missing even though CI installed it.
    python -m platformio run
    if ($LASTEXITCODE -ne 0) { throw "Firmware build failed." }

    $Build = Join-Path $Root ".pio\build\cardputer-adv"
    $Firmware = Join-Path $Build "firmware.bin"

    # The OTA slot is 3 MiB, but MAZ Pocket deliberately gets much less. The
    # unused space is future headroom, not permission to accumulate apps.
    $MaxFirmwareBytes = 2100000
    $FirmwareBytes = (Get-Item $Firmware).Length
    if ($FirmwareBytes -gt $MaxFirmwareBytes) {
        throw "Firmware bloat gate failed: $FirmwareBytes bytes > $MaxFirmwareBytes byte v0.3 budget. Remove or simplify features."
    }
    Write-Host "Firmware budget: $FirmwareBytes / $MaxFirmwareBytes bytes"

    $Dist = Join-Path $Root "dist"
    New-Item -ItemType Directory -Force $Dist | Out-Null
    Copy-Item $Firmware (Join-Path $Dist "maz-pocket-app.bin") -Force
    Remove-Item (Join-Path $Dist "maz-pocket-merged.bin") -Force -ErrorAction SilentlyContinue
} finally {
    Pop-Location
}
