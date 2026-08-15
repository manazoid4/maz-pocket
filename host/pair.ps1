param(
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"
$HostRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$EnvPath = Join-Path $HostRoot ".env"

if (-not (Test-Path $EnvPath)) {
    throw "Run install-core.ps1 first."
}

$TokenMatch = Select-String -Path $EnvPath -Pattern '^MAZ_TOKEN=(.+)$' | Select-Object -First 1
if (-not $TokenMatch) { throw "MAZ_TOKEN is missing from .env" }
$Token = $TokenMatch.Matches[0].Groups[1].Value.Trim()

$Address = Get-NetIPAddress -AddressFamily IPv4 | Where-Object {
    $_.InterfaceAlias -eq "Wi-Fi" -and $_.IPAddress -notlike "127.*" -and $_.IPAddress -notlike "169.254.*"
} | Select-Object -First 1 -ExpandProperty IPAddress
if (-not $Address) {
    $Address = Get-NetIPAddress -AddressFamily IPv4 | Where-Object {
        $_.IPAddress -notlike "127.*" -and $_.IPAddress -notlike "169.254.*" -and $_.PrefixOrigin -ne "WellKnown"
    } | Select-Object -First 1 -ExpandProperty IPAddress
}
if (-not $Address) { throw "No usable PC LAN address found." }

$PortName = Get-CimInstance Win32_SerialPort | Where-Object {
    $_.PNPDeviceID -match 'VID_303A&PID_1001'
} | Select-Object -First 1 -ExpandProperty DeviceID
if (-not $PortName) { throw "Cardputer ADV not found over USB." }

$Port = [System.IO.Ports.SerialPort]::new($PortName, 115200)
$Port.NewLine = "`n"
$Port.ReadTimeout = 500
try {
    $Port.Open()
    Start-Sleep -Milliseconds 300
    $Port.DiscardInBuffer()
    # v0.6 pairing updates only MAZ Core address/token. It deliberately leaves
    # the Cardputer's already-working Wi-Fi credentials untouched.
    $Port.WriteLine("MAZCOREPAIR`t$Address`t8787`t$Token")
    $Deadline = [DateTime]::UtcNow.AddSeconds(8)
    $Reply = ""
    do {
        try {
            $Reply = $Port.ReadLine().Trim()
        } catch [System.TimeoutException] {
            continue
        }
    } while ($Reply -notlike 'MAZCOREPAIR *' -and [DateTime]::UtcNow -lt $Deadline)

    if ($Reply -notlike 'MAZCOREPAIR OK*') {
        throw "Device did not accept Core-only pairing. Install MAZ Pocket v0.6 first or pair once from CONTROL > MAZ Core. Reply: $Reply"
    }
    if (-not $Quiet) {
        Write-Host "MAZ Pocket paired to MAZ Core at ${Address}:8787 without changing Wi-Fi." -ForegroundColor Green
    }
} finally {
    if ($Port.IsOpen) { $Port.Close() }
}
