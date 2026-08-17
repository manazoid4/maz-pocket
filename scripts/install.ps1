param(
    [string]$Binary = "",
    [string]$Port = ""
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$HostPython = Join-Path $Root "host\.venv\Scripts\python.exe"

if (-not $Binary) {
    & (Join-Path $Root "scripts\package-release.ps1")
    $Binary = Join-Path $Root "dist\maz-pocket-app.bin"
}
if (-not (Test-Path $Binary)) { throw "Firmware not found: $Binary" }

# Refuse to install an image older than the current build. An agent or a stale
# shell re-running this with a hand-rolled -Binary would otherwise quietly
# overwrite freshly flashed firmware with whatever was last packaged, and the
# device gives no sign at all that it happened.
$Built = Join-Path $Root ".pio\build\cardputer-adv\firmware.bin"
if (Test-Path $Built) {
    $BuiltAt = (Get-Item $Built).LastWriteTimeUtc
    $ImageAt = (Get-Item $Binary).LastWriteTimeUtc
    if ($BuiltAt -gt $ImageAt.AddSeconds(1)) {
        throw ("Refusing to flash a stale image. '$Binary' was packaged at " +
               "$ImageAt UTC, but .pio\build\cardputer-adv\firmware.bin is " +
               "from $BuiltAt UTC. Run scripts\package-release.ps1 first, or " +
               "pass -Binary .pio\build\cardputer-adv\firmware.bin.")
    }
}

if (-not (Test-Path $HostPython)) {
    & (Join-Path $Root "host\setup.ps1")
}
& $HostPython -m pip install -q -r (Join-Path $Root "host\requirements.txt")

if (-not $Port) {
    for ($attempt = 0; $attempt -lt 20 -and -not $Port; $attempt++) {
        $Port = Get-CimInstance Win32_SerialPort | Where-Object {
            $_.PNPDeviceID -match 'VID_303A&PID_1001'
        } | Select-Object -First 1 -ExpandProperty DeviceID
        if (-not $Port) { Start-Sleep -Seconds 1 }
    }
}
if (-not $Port) {
    throw "Cardputer ADV not found over USB after 20 seconds. Connect its data cable and tap RESET."
}

# If MAZ Pocket is currently open, wait through the serial-open reset and then
# ask it to hand back to Launcher. A Launcher already running simply ignores
# this best-effort step and is accepted by `prepare` below.
try {
    & $HostPython (Join-Path $Root "scripts\launcher-device.py") handoff --port $Port
} catch {
}

# Tell preparation how big the image is. M5Launcher answers a `flash firmware`
# command it cannot satisfy with silence, which the flasher can only report as
# "Timed out waiting for READY" -- a space problem that reads like a cable fault.
$ImageBytes = (Get-Item $Binary).Length
& $HostPython (Join-Path $Root "scripts\launcher-device.py") prepare --port $Port --require-free $ImageBytes
if ($LASTEXITCODE -ne 0) { throw "M5Launcher preparation failed." }

$ToolDir = Join-Path ([IO.Path]::GetTempPath()) "maz-pocket-m5launcher-2.8.0"
$Flasher = Join-Path $ToolDir "serial_flasher.py"
New-Item -ItemType Directory -Force $ToolDir | Out-Null
if (-not (Test-Path $Flasher)) {
    Invoke-WebRequest `
        "https://raw.githubusercontent.com/bmorcelli/M5Stick-Launcher/2.8.0/tools/serial_flasher.py" `
        -OutFile $Flasher
}
$Expected = "9CFBA9AF762AC7D99488F23706320B30D0896C4993599990EC80AD06AB8F7536"
$Actual = (Get-FileHash $Flasher -Algorithm SHA256).Hash
if ($Actual -ne $Expected) { throw "Downloaded M5Launcher tool failed its checksum." }

& $HostPython $Flasher -f $Binary -p $Port -n "MAZ-Pocket"
$FlashResult = $LASTEXITCODE

# Launcher 2.8.0 can reboot before its final serial OK drains on Windows. The
# actual acceptance criterion is the freshly installed firmware booting.
& $HostPython (Join-Path $Root "scripts\launcher-device.py") verify --port $Port
if ($LASTEXITCODE -ne 0) { throw "MAZ Pocket did not boot after M5Launcher install (flasher exit $FlashResult)." }
