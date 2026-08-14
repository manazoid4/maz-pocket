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
    Remove-Item (Join-Path $Dist "maz-pocket-merged.bin") -Force -ErrorAction SilentlyContinue
} finally {
    Pop-Location
}
