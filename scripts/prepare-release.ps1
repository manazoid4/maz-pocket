$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Version = (Get-Content (Join-Path $Root "VERSION") -Raw).Trim()
$Dist = Join-Path $Root "dist"
$SourceFirmware = Join-Path $Dist "maz-pocket-app.bin"
$FirmwareName = "Maz-Pocket-v$Version-M5Launcher.bin"
$CoreName = "MAZ-Core-v$Version.zip"
$CardputerName = "MAZ-Cardputer-v$Version.zip"
$BundleName = "MAZ-Pocket-v$Version-Install.zip"
$Firmware = Join-Path $Dist $FirmwareName
$CoreZip = Join-Path $Dist $CoreName
$CardputerZip = Join-Path $Dist $CardputerName
$Bundle = Join-Path $Dist $BundleName

if (-not (Test-Path $SourceFirmware -PathType Leaf)) { throw "Build firmware first: $SourceFirmware not found" }
$bytes = [IO.File]::ReadAllBytes($SourceFirmware)
if ($bytes.Length -lt 65536 -or $bytes[0] -ne 0xE9) { throw "Invalid ESP32 app image" }
if ($bytes.Length -gt 0x180000) { throw "Firmware exceeds known M5Launcher app slot: $($bytes.Length)" }

New-Item -ItemType Directory -Force $Dist | Out-Null
Copy-Item $SourceFirmware $Firmware -Force

# Client/laptop package. Keep VERSION beside the host so the Python runtime has
# one identity source in both a repo checkout and an extracted release ZIP.
$CoreStage = Join-Path $Dist "core-package"
Remove-Item $CoreStage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $CoreStage | Out-Null
Get-ChildItem (Join-Path $Root "host") -Force | ForEach-Object {
    if ($_.Name -ne ".env" -and $_.Name -ne ".venv" -and $_.Name -ne "__pycache__") {
        Copy-Item $_.FullName $CoreStage -Recurse -Force
    }
}
Copy-Item (Join-Path $Root "VERSION") (Join-Path $CoreStage "VERSION") -Force

# Ship the runtime, not CI leftovers. Tests remain in the repository/Actions;
# the end-user Core ZIP should not carry pytest caches, bytecode or test trees.
Remove-Item (Join-Path $CoreStage "tests") -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $CoreStage ".pytest_cache") -Recurse -Force -ErrorAction SilentlyContinue
Get-ChildItem $CoreStage -Recurse -Directory -Force |
    Where-Object { $_.Name -eq "__pycache__" } |
    Sort-Object FullName -Descending |
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
Get-ChildItem $CoreStage -Recurse -File -Force -Filter "*.pyc" |
    Remove-Item -Force -ErrorAction SilentlyContinue

Remove-Item $CoreZip -Force -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $CoreStage "*") -DestinationPath $CoreZip -Force

$PackageFiles = @{
    "VERSION" = (Join-Path $Root "VERSION")
    "START-HERE.cmd" = (Join-Path $Root "scripts\START-HERE.cmd")
    "setup-all.ps1" = (Join-Path $Root "scripts\setup-all.ps1")
    "INSTALL-MAZ-POCKET.cmd" = (Join-Path $Root "scripts\INSTALL-MAZ-POCKET.cmd")
    "install-to-sd.ps1" = (Join-Path $Root "scripts\install-to-sd.ps1")
    "QUICKSTART.txt" = (Join-Path $Root "QUICKSTART.txt")
    "RELEASE_NOTES.md" = (Join-Path $Root "RELEASE_NOTES.md")
}
foreach ($entry in $PackageFiles.GetEnumerator()) {
    if (-not (Test-Path $entry.Value -PathType Leaf)) { throw "Release input missing: $($entry.Value)" }
    Copy-Item $entry.Value (Join-Path $Dist $entry.Key) -Force
}

# Cardputer-only package rule: every release/PR artifact must contain a complete
# handheld ZIP as well as the client ZIP. This is deliberately app-only;
# M5Launcher remains the installer and rollback owner.
$CardStage = Join-Path $Dist "cardputer-package"
Remove-Item $CardStage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $CardStage | Out-Null
Copy-Item $Firmware (Join-Path $CardStage $FirmwareName) -Force
Copy-Item (Join-Path $Dist "VERSION") $CardStage -Force
Copy-Item (Join-Path $Dist "INSTALL-MAZ-POCKET.cmd") $CardStage -Force
Copy-Item (Join-Path $Dist "install-to-sd.ps1") $CardStage -Force
Copy-Item (Join-Path $Dist "QUICKSTART.txt") $CardStage -Force
Copy-Item (Join-Path $Dist "RELEASE_NOTES.md") $CardStage -Force
$FirmwareHash = (Get-FileHash $Firmware -Algorithm SHA256).Hash.ToLowerInvariant()
"$FirmwareHash  $FirmwareName" | Set-Content -Encoding ascii (Join-Path $CardStage "SHA256-FIRMWARE.txt")
Remove-Item $CardputerZip -Force -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $CardStage "*") -DestinationPath $CardputerZip -Force

$HashTargets = @(
    $Firmware,
    $CoreZip,
    $CardputerZip,
    (Join-Path $Dist "START-HERE.cmd"),
    (Join-Path $Dist "setup-all.ps1"),
    (Join-Path $Dist "install-to-sd.ps1")
)
$HashLines = foreach ($file in $HashTargets) {
    $hash = (Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $([IO.Path]::GetFileName($file))"
}
$HashLines | Set-Content -Encoding ascii (Join-Path $Dist "SHA256SUMS.txt")

# Combined convenience ZIP contains BOTH independently usable ZIPs. Never make
# users extract source trees or reconstruct a host/card split by hand.
$InstallStage = Join-Path $Dist "install-package"
Remove-Item $InstallStage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $InstallStage | Out-Null
$InstallNames = @(
    $FirmwareName,
    $CoreName,
    $CardputerName,
    "VERSION",
    "START-HERE.cmd",
    "setup-all.ps1",
    "INSTALL-MAZ-POCKET.cmd",
    "install-to-sd.ps1",
    "SHA256SUMS.txt",
    "QUICKSTART.txt",
    "RELEASE_NOTES.md"
)
foreach ($name in $InstallNames) { Copy-Item (Join-Path $Dist $name) $InstallStage -Force }
Remove-Item $Bundle -Force -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $InstallStage "*") -DestinationPath $Bundle -Force

Write-Host "Prepared MAZ Pocket v$Version release:"
Get-ChildItem $Firmware, $CoreZip, $CardputerZip, $Bundle, (Join-Path $Dist "SHA256SUMS.txt") | Select-Object Name, Length
Get-Content (Join-Path $Dist "SHA256SUMS.txt")
