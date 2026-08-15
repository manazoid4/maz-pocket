$ErrorActionPreference = "Stop"
$HostRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Python = Join-Path $HostRoot ".venv\Scripts\python.exe"
$LogDir = Join-Path $HostRoot "logs"
New-Item -ItemType Directory -Force $LogDir | Out-Null
$StartupLog = Join-Path $LogDir "core-startup.log"
if (-not (Test-Path $Python)) { throw "Run INSTALL.cmd first." }

try {
    & (Join-Path $HostRoot "start-local-ai.ps1")
    "$(Get-Date -Format o) local-ai supervisor ready" | Add-Content -Encoding utf8 $StartupLog
} catch {
    "$(Get-Date -Format o) local-ai supervisor failed: $($_.Exception.Message)" | Add-Content -Encoding utf8 $StartupLog
}

Push-Location $HostRoot
try {
    & $Python -c "import uvicorn; from mazhost.config import Settings; s = Settings(); uvicorn.run('mazhost.app:app', host=s.bind, port=s.port, log_level='warning')"
} finally {
    Pop-Location
}
