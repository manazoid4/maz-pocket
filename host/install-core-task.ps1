# Installs/refreshes nod Core as a logon Scheduled Task. Safe to re-run.
#   .\install-core-task.ps1 [-Fw path\to\firmware.bin]
# Copies mazhost + VERSION into %LOCALAPPDATA%\MAZ Core, keeps .env/.venv/data, stages firmware,
# kills every other Core process, (re)starts the task, waits for /health.
param([string]$Fw, [string]$Install = (Join-Path $env:LOCALAPPDATA "MAZ Core"))
$ErrorActionPreference = "Stop"
$Src = Split-Path -Parent $MyInvocation.MyCommand.Path
$Repo = Split-Path -Parent $Src
$Task = "nod Core"
$Py = Join-Path $Install ".venv\Scripts\pythonw.exe"
if (-not (Test-Path $Py)) { throw "No venv at $Install. Run setup.ps1 there first." }

# 1. stop task + any stray Core (temp runcore.py, run.ps1, old uvicorn): anything on the Core port
Stop-ScheduledTask -TaskName $Task -ErrorAction SilentlyContinue
$port = 8787
$pids = @(Get-NetTCPConnection -LocalPort $port -State Listen -ErrorAction SilentlyContinue | ForEach-Object OwningProcess)
$pids += Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match "runcore\.py|-m mazhost|mazhost\.app" } | ForEach-Object ProcessId
$pids | Sort-Object -Unique | Where-Object { $_ -and $_ -ne $PID } | ForEach-Object { cmd /c "taskkill /PID $_ /T /F >nul 2>&1" }

# 2. copy code
Remove-Item (Join-Path $Install "mazhost") -Recurse -Force -ErrorAction SilentlyContinue
Copy-Item (Join-Path $Src "mazhost") $Install -Recurse -Force
Get-ChildItem $Install -Recurse -Directory -Filter __pycache__ | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
$ver = Join-Path $Repo "VERSION"
if (Test-Path $ver) { Copy-Item $ver (Join-Path $Install "VERSION") -Force }
foreach ($f in "install-core-task.ps1") {
    if (Test-Path (Join-Path $Src $f)) { Copy-Item (Join-Path $Src $f) $Install -Force }
}

# 3. firmware: explicit -Fw, else a prebuilt host\fw, else the repo build
$fwOut = Join-Path $Install "fw"
if (-not $Fw) {
    $Fw = Join-Path $Repo ".pio\build\cardputer-adv\firmware.bin"
    if (-not (Test-Path $Fw)) { $Fw = $null }
}
if ($Fw) { & (Join-Path $env:LOCALAPPDATA "hermes\hermes-agent\venv\Scripts\python.exe") (Join-Path $Repo "scripts\fw_manifest.py") $Fw $fwOut }

# 4. scheduled task: at logon, restart on failure, no time limit
$act = New-ScheduledTaskAction -Execute $Py -Argument "-m mazhost" -WorkingDirectory $Install
$trg = New-ScheduledTaskTrigger -AtLogOn -User $env:USERNAME
$set = New-ScheduledTaskSettingsSet -RestartCount 99 -RestartInterval (New-TimeSpan -Minutes 1) `
    -ExecutionTimeLimit ([TimeSpan]::Zero) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable
Register-ScheduledTask -TaskName $Task -Action $act -Trigger $trg -Settings $set -RunLevel Limited -Force | Out-Null
Start-ScheduledTask -TaskName $Task

# 5. desktop shortcut "nod Core status" -> /health
$lnk = Join-Path ([Environment]::GetFolderPath("Desktop")) "nod Core status.url"
"[InternetShortcut]`nURL=http://127.0.0.1:$port/health" | Set-Content $lnk -Encoding ascii

# 6. wait for health
$token = (Select-String -Path (Join-Path $Install ".env") -Pattern "^MAZ_TOKEN=(.*)$" | Select-Object -First 1).Matches[0].Groups[1].Value.Trim()
for ($i = 0; $i -lt 60; $i++) {
    try {
        $h = Invoke-RestMethod "http://127.0.0.1:$port/health" -Headers @{ Authorization = "Bearer $token" } -TimeoutSec 15
        "nod Core v$($h.version) up under task '$Task'; fw_latest=$($h.fw_latest.version) ($($h.fw_latest.sha))"; exit 0
    } catch { $err = $_.Exception.Message; Start-Sleep 1 }
}
throw "Core did not come up ($err); see $Install\logs\core.log"
