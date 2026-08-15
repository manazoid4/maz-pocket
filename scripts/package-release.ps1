$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Push-Location $Root
try {
    # Use the active interpreter, not the Windows `py` launcher. GitHub runners
    # can otherwise select a different Python than setup-python installed into.
    python -m platformio run
    if ($LASTEXITCODE -ne 0) { throw "Firmware build failed." }

    $Build = Join-Path $Root ".pio\build\cardputer-adv"
    $Firmware = Join-Path $Build "firmware.bin"
    $FirmwareBytes = (Get-Item $Firmware).Length

    # Preserve the known live M5Launcher app-slot ceiling. MAZ Pocket is an app
    # image: releases must slim down rather than repartitioning a user's device
    # or deleting sibling Launcher apps to make a build fit.
    $LauncherSlotBytes = 0x180000
    if ($FirmwareBytes -gt $LauncherSlotBytes) {
        throw "M5Launcher compatibility gate failed: $FirmwareBytes bytes > $LauncherSlotBytes byte app slot. Slim firmware; do not repartition the user's device."
    }
    Write-Host "M5Launcher app slot: $FirmwareBytes / $LauncherSlotBytes bytes"

    $Dist = Join-Path $Root "dist"
    New-Item -ItemType Directory -Force $Dist | Out-Null
    Copy-Item $Firmware (Join-Path $Dist "maz-pocket-app.bin") -Force
    Remove-Item (Join-Path $Dist "maz-pocket-merged.bin") -Force -ErrorAction SilentlyContinue
} finally {
    Pop-Location
}
