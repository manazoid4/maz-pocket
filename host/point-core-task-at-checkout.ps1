# Re-registers the "nod Core" task so Core runs mazhost straight from a git checkout (so self-update can
# fast-forward it), while keeping .env, logs, data and staged firmware in the install dir.
param([string]$Repo, [string]$Install = (Join-Path $env:LOCALAPPDATA "MAZ Core"))
$ErrorActionPreference = "Stop"
$Task = "nod Core"
$Py = Join-Path $Install ".venv\Scripts\pythonw.exe"
$code = "import os,sys; os.environ.setdefault('MAZ_FW_DIR', r'$Install\fw'); os.environ.setdefault('MAZ_REPO_DIR', r'$Repo'); sys.path.insert(0, r'$Repo\host'); from mazhost.__main__ import main; main()"
$act = New-ScheduledTaskAction -Execute $Py -Argument ('-c "' + $code + '"') -WorkingDirectory $Install
$trg = New-ScheduledTaskTrigger -AtLogOn -User $env:USERNAME
$set = New-ScheduledTaskSettingsSet -RestartCount 99 -RestartInterval (New-TimeSpan -Minutes 1) `
    -ExecutionTimeLimit ([TimeSpan]::Zero) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable
Register-ScheduledTask -TaskName $Task -Action $act -Trigger $trg -Settings $set -RunLevel Limited -Force | Out-Null
"Task '$Task' now runs mazhost from $Repo (state in $Install)"
