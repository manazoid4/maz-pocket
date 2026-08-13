$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Push-Location $Root
try {
    py -m platformio run
    if ($LASTEXITCODE -ne 0) { throw "Firmware build failed." }
    $Build = Join-Path $Root ".pio\build\cardputer-adv"
    $Dist = Join-Path $Root "dist"
    New-Item -ItemType Directory -Force $Dist | Out-Null
    Copy-Item (Join-Path $Build "firmware.bin") (Join-Path $Dist "maz-pocket-app.bin") -Force
    Copy-Item (Join-Path $Build "firmware.bin") (Join-Path $Root "flash\maz-pocket-app.bin") -Force
    py -m platformio pkg exec -p tool-esptoolpy -- esptool.py --chip esp32s3 merge_bin --flash_mode keep --flash_freq 80m --flash_size 8MB `
        -o (Join-Path $Dist "maz-pocket-merged.bin") `
        0x0 (Join-Path $Build "bootloader.bin") `
        0x8000 (Join-Path $Build "partitions.bin") `
        0xe000 (Join-Path $env:USERPROFILE ".platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin") `
        0x10000 (Join-Path $Build "firmware.bin")
    if ($LASTEXITCODE -ne 0) { throw "Merged image packaging failed." }
    Copy-Item (Join-Path $Dist "maz-pocket-merged.bin") (Join-Path $Root "flash\maz-pocket-merged.bin") -Force
} finally {
    Pop-Location
}
