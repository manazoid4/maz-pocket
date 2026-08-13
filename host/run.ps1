$ErrorActionPreference = "Stop"
$HostRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Python = Join-Path $HostRoot ".venv\Scripts\python.exe"
if (-not (Test-Path $Python)) { throw "Run .\setup.ps1 first." }
Push-Location $HostRoot
try {
    & $Python -m uvicorn mazhost.app:app --host 0.0.0.0 --port 8787
} finally {
    Pop-Location
}
