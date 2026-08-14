$ErrorActionPreference = "Stop"
$HostRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Python = Join-Path $HostRoot ".venv\Scripts\python.exe"
if (-not (Test-Path $Python)) { throw "Run .\setup.ps1 first." }
Push-Location $HostRoot
try {
    & $Python -c "import uvicorn; from mazhost.config import Settings; s = Settings(); uvicorn.run('mazhost.app:app', host=s.bind, port=s.port)"
} finally {
    Pop-Location
}
