$ErrorActionPreference = "Stop"
$HostRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$EnvPath = Join-Path $HostRoot ".env"

if (-not (Test-Path $EnvPath)) {
    throw "Run .\setup.ps1 first."
}

$Token = (Select-String -Path $EnvPath -Pattern '^MAZ_TOKEN=(.+)$').Matches.Groups[1].Value
$Address = Get-NetIPAddress -AddressFamily IPv4 | Where-Object {
    $_.InterfaceAlias -eq "Wi-Fi" -and $_.IPAddress -notlike "169.254.*"
} | Select-Object -First 1 -ExpandProperty IPAddress
if (-not $Address) { throw "The laptop is not connected to Wi-Fi." }

$WifiName = (Get-NetConnectionProfile | Where-Object {
    $_.InterfaceAlias -eq "Wi-Fi" -and $_.IPv4Connectivity -ne "Disconnected"
} | Select-Object -First 1 -ExpandProperty Name)
if (-not $WifiName) { $WifiName = Read-Host "Wi-Fi name" }

$SecurePassword = Read-Host "Wi-Fi password for $WifiName" -AsSecureString
$PasswordPointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($SecurePassword)
try {
    $WifiPassword = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($PasswordPointer)
} finally {
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($PasswordPointer)
}

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
    $Port.WriteLine("MAZPAIR`t$WifiName`t$WifiPassword`t$Address`t8787`t$Token")
    $Deadline = [DateTime]::UtcNow.AddSeconds(15)
    $Reply = ""
    do {
        try {
            $Reply = $Port.ReadLine()
        } catch [System.TimeoutException] {
            continue
        }
    } while ($Reply -notlike 'MAZPAIR *' -and [DateTime]::UtcNow -lt $Deadline)
    if ($Reply -notlike 'MAZPAIR OK*') { throw "Device pairing failed: $Reply" }
    Write-Host "MAZ Pocket paired to MAZ Host at ${Address}:8787."
} finally {
    if ($Port.IsOpen) { $Port.Close() }
    $WifiPassword = $null
}
