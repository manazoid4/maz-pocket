# Detached helper started by Core self-update. Restarts the "nod Core" task, waits for /health to report
# the new git sha; if it never does, rolls the checkout back to -PrevSha, reinstalls deps, restarts again.
param([string]$Repo, [string]$Python, [string]$NewSha, [string]$PrevSha, [int]$Port = 8787,
      [string]$EnvFile, [string]$LogDir, [string]$Task = "nod Core", [int]$WaitSeconds = 90)
$ErrorActionPreference = "Continue"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$Log = Join-Path $LogDir "selfupdate.log"
function Note($m) { Add-Content -Path $Log -Value ("{0} restart-core[{1}]: {2}" -f (Get-Date -Format s), $PID, $m) -Encoding ascii }

function Restart-Task {
    schtasks /End /TN $Task 2>&1 | Out-Null
    Start-Sleep -Seconds 3
    # make sure nothing still holds the port
    Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue |
        ForEach-Object { cmd /c "taskkill /PID $($_.OwningProcess) /T /F >nul 2>&1" }
    schtasks /Run /TN $Task 2>&1 | Out-Null
}

function Wait-Health([string]$Sha) {
    $token = ""
    if ($EnvFile -and (Test-Path $EnvFile)) {
        $line = Select-String -Path $EnvFile -Pattern "^MAZ_TOKEN=(.*)$" | Select-Object -First 1
        if ($line) { $token = $line.Matches[0].Groups[1].Value.Trim() }
    }
    $deadline = (Get-Date).AddSeconds($WaitSeconds)
    while ((Get-Date) -lt $deadline) {
        try {
            $h = Invoke-RestMethod "http://127.0.0.1:$Port/health" -Headers @{ Authorization = "Bearer $token" } -TimeoutSec 5
            if ($h.git_sha -eq $Sha) { return $true }
        } catch { }
        Start-Sleep -Seconds 2
    }
    return $false
}

Note "restarting into $NewSha (previous $PrevSha)"
Restart-Task
if (Wait-Health $NewSha) { Note "ok: Core healthy on $NewSha"; exit 0 }

Note "ROLLBACK: Core not healthy on $NewSha after $WaitSeconds s; reverting to $PrevSha"
Add-Content -Path (Join-Path $LogDir "selfupdate-bad.txt") -Value $NewSha -Encoding ascii
git -C $Repo checkout --detach $PrevSha 2>&1 | ForEach-Object { Note "git: $_" }
& $Python -m pip install -r (Join-Path $Repo "host\requirements.txt") 2>&1 | Select-Object -Last 3 | ForEach-Object { Note "pip: $_" }
Restart-Task
if (Wait-Health $PrevSha) { Note "rollback ok: Core healthy on $PrevSha" } else { Note "rollback FAILED: Core not healthy on $PrevSha; manual attention needed" }
