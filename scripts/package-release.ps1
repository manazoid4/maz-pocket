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

    # The physically accepted v0.02 preview binary was 1,525,984 bytes. Current
    # M5Launcher app partitions are aligned to 0x10000, so that installation has
    # a 0x180000 (1,572,864 byte) slot. v0.03 MUST fit that live slot: the safe
    # web flasher never repartitions the user's device or deletes sibling apps.
    $V02LauncherSlotBytes = 0x180000
    if ($FirmwareBytes -gt $V02LauncherSlotBytes) {
        throw "v0.02 compatibility gate failed: $FirmwareBytes bytes > $V02LauncherSlotBytes byte live Launcher slot. Slim firmware; do not repartition the user's device."
    }
    Write-Host "v0.02 Launcher slot: $FirmwareBytes / $V02LauncherSlotBytes bytes"

    $Dist = Join-Path $Root "dist"
    New-Item -ItemType Directory -Force $Dist | Out-Null
    Copy-Item $Firmware (Join-Path $Dist "maz-pocket-app.bin") -Force
    Remove-Item (Join-Path $Dist "maz-pocket-merged.bin") -Force -ErrorAction SilentlyContinue
} finally {
    Pop-Location
}
