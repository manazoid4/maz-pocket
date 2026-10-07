# nod-setup.ps1 - ONE installer for nod Core on Windows (called by nod-setup.cmd). ASCII only, PS 5.1 safe.
# Idempotent: re-running updates everything in place and never asks a question.
# The secret MAZ_TOKEN is generated once into %LOCALAPPDATA%\MAZ Core\.env and is NEVER printed.
param(
    [switch]$FirewallOnly,   # internal: elevated child that only creates the firewall rules
    [int]$Port = 8787
)

$ErrorActionPreference = "Stop"
$Task = "nod Core"
$Install = Join-Path $env:LOCALAPPDATA "MAZ Core"
$EnvFile = Join-Path $Install ".env"
$RepoUrl = "https://github.com/manazoid4/maz-pocket.git"
$Branch = "deploy/local"

function Step([string]$m) { Write-Host ""; Write-Host "== $m" -ForegroundColor Cyan }
function Note([string]$m) { Write-Host "   $m" }
function Warn([string]$m) { Write-Host "   [!] $m" -ForegroundColor Yellow }

function Test-Admin {
    $p = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Update-SessionPath {
    $m = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $u = [Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = "$m;$u"
}

# ---- firewall (the only step that needs admin) --------------------------------------------------
function Get-MissingFirewallRules {
    $missing = @()
    $tcp = Get-NetFirewallRule -DisplayName "nod Core TCP $Port" -ErrorAction SilentlyContinue
    $old = Get-NetFirewallRule -DisplayName "MAZ Core $Port" -ErrorAction SilentlyContinue
    if (-not $tcp -and -not $old) { $missing += "tcp" }
    $udp = Get-NetFirewallRule -DisplayName "nod Core UDP $Port" -ErrorAction SilentlyContinue
    if (-not $udp) { $missing += "udp" }
    return $missing
}

function New-FirewallRules {
    foreach ($m in (Get-MissingFirewallRules)) {
        $proto = $m.ToUpper()
        New-NetFirewallRule -DisplayName "nod Core $proto $Port" -Direction Inbound -Action Allow `
            -Protocol $proto -LocalPort $Port -Profile Private | Out-Null
    }
}

if ($FirewallOnly) {
    New-FirewallRules
    exit 0
}

function Find-Python {
    foreach ($name in @("py", "python")) {
        $c = Get-Command $name -ErrorAction SilentlyContinue
        if (-not $c) { continue }
        $args1 = @()
        if ($name -eq "py") { $args1 = @("-3") }
        try {
            $v = & $c.Source @args1 -c "import sys; print(sys.version_info[0]*100+sys.version_info[1])" 2>$null
            if ([int]$v -ge 310) {
                $exe = & $c.Source @args1 -c "import sys; print(sys.executable)"
                return $exe.Trim()
            }
        } catch { }
    }
    return $null
}

function Install-WithWinget([string]$Id, [string]$What) {
    $w = Get-Command winget -ErrorAction SilentlyContinue
    if (-not $w) { throw "$What is needed and winget is not available. Install $What from its website, then double-click nod-setup.cmd again." }
    Note "Installing $What (one time, may take a minute)..."
    & winget install --id $Id -e --silent --accept-package-agreements --accept-source-agreements | Out-Null
    Update-SessionPath
}

function Read-EnvValue([string]$Name) {
    if (-not (Test-Path $EnvFile)) { return "" }
    foreach ($line in [IO.File]::ReadAllLines($EnvFile)) {
        if ($line.StartsWith("$Name=")) { return $line.Substring($Name.Length + 1).Trim() }
    }
    return ""
}

function Set-EnvValue([string]$Name, [string]$Value, [switch]$OnlyIfMissing) {
    $lines = @()
    if (Test-Path $EnvFile) { $lines = @([IO.File]::ReadAllLines($EnvFile)) }
    $found = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i].StartsWith("$Name=")) {
            $found = $true
            if (-not $OnlyIfMissing -or $lines[$i].Substring($Name.Length + 1).Trim() -eq "") {
                $lines[$i] = "$Name=$Value"
            }
        }
    }
    if (-not $found) { $lines += "$Name=$Value" }
    [IO.File]::WriteAllLines($EnvFile, [string[]]$lines, (New-Object Text.UTF8Encoding($false)))
}

function Wait-Health([int]$Seconds) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        try {
            $h = Invoke-RestMethod "http://127.0.0.1:$Port/health" -TimeoutSec 5
            if ($h.ok) { return $h }
        } catch { }
        Start-Sleep -Seconds 2
    }
    return $null
}

function Get-LanAddress {
    $a = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue | Where-Object {
        $_.IPAddress -notlike "127.*" -and $_.IPAddress -notlike "169.254.*" -and $_.PrefixOrigin -ne "WellKnown"
    } | Select-Object -First 1 -ExpandProperty IPAddress
    return $a
}

$Ok = $false
$Code = ""
try {
    Write-Host ""
    Write-Host "nod setup - one click, no questions." -ForegroundColor Green
    New-Item -ItemType Directory -Force -Path $Install | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $Install "logs") | Out-Null
    try { Start-Transcript -Path (Join-Path $Install "logs\nod-setup.log") -Append | Out-Null } catch { }
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

    # ---- 1. the code checkout (Core self-update fast-forwards this folder) ----------------------
    Step "1/7 Finding nod"
    $Repo = Split-Path -Parent $PSScriptRoot
    if (-not (Test-Path (Join-Path $Repo "host\mazhost\app.py"))) {
        $Repo = Join-Path $env:LOCALAPPDATA "nod\maz-pocket"
        if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Install-WithWinget "Git.Git" "Git" }
        if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw "Git did not install. Install Git from git-scm.com, then run nod-setup.cmd again." }
        if (Test-Path (Join-Path $Repo ".git")) {
            Note "Using existing copy in $Repo"
        } else {
            Note "Downloading nod to $Repo"
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Repo) | Out-Null
            & git clone --branch $Branch $RepoUrl $Repo
            if ($LASTEXITCODE -ne 0) { throw "Could not download nod from GitHub. Check your internet, then run nod-setup.cmd again." }
        }
    } else {
        Note "Using $Repo"
        if (-not (Test-Path (Join-Path $Repo ".git"))) { Warn "This copy has no .git folder, so automatic updates are off. Clone the repo to get them." }
    }
    $HostDir = Join-Path $Repo "host"

    # ---- 2. python + venv + dependencies --------------------------------------------------------
    Step "2/7 Python and libraries"
    $Py = Find-Python
    if (-not $Py) {
        Install-WithWinget "Python.Python.3.12" "Python 3.12"
        $Py = Find-Python
        if (-not $Py) { throw "Python did not install. Install Python 3.11+ from python.org (tick 'Add to PATH'), then run nod-setup.cmd again." }
    }
    $VenvPy = Join-Path $Install ".venv\Scripts\python.exe"
    $VenvPyw = Join-Path $Install ".venv\Scripts\pythonw.exe"
    if (-not (Test-Path $VenvPy)) {
        Note "Creating the private Python environment"
        & $Py -m venv (Join-Path $Install ".venv")
        if ($LASTEXITCODE -ne 0) { throw "Could not create the Python environment." }
    }
    Note "Installing libraries (first run takes a few minutes)"
    & $VenvPy -m pip install --disable-pip-version-check --quiet -r (Join-Path $HostDir "requirements.txt")
    if ($LASTEXITCODE -ne 0) { throw "Installing libraries failed. Check your internet, then run nod-setup.cmd again." }

    # ---- 3. the secret + settings (never printed) ------------------------------------------------
    Step "3/7 Settings"
    if (-not (Test-Path $EnvFile)) {
        Copy-Item (Join-Path $HostDir ".env.example") $EnvFile
    }
    $tok = Read-EnvValue "MAZ_TOKEN"
    if (-not $tok -or $tok -eq "change-me-before-first-run") {
        $new = (& $VenvPy -c "import secrets; print(secrets.token_urlsafe(24))").Trim()
        Set-EnvValue "MAZ_TOKEN" $new
        $new = $null
        Note "Created your private key (kept on this PC only)"
    } else {
        Note "Private key already present - kept"
    }
    $tok = $null
    $Desktop = Join-Path $env:USERPROFILE "Desktop"
    Set-EnvValue "MAZ_CORE_ENABLED" "true" -OnlyIfMissing
    Set-EnvValue "MAZ_PROJECT_ROOTS" $Desktop -OnlyIfMissing
    Set-EnvValue "MAZ_CARDPUTER_URL" "http://mazpocket.local" -OnlyIfMissing

    # ---- 4. Scheduled Task "nod Core" running from the checkout -----------------------------------
    Step "4/7 Starting nod Core"
    Stop-ScheduledTask -TaskName $Task -ErrorAction SilentlyContinue
    $pids = @(Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue | ForEach-Object { $_.OwningProcess })
    $pids += Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandLine -match "runcore\.py|-m mazhost|mazhost\.app|mazhost\.__main__" } | ForEach-Object { $_.ProcessId }
    $pids | Sort-Object -Unique | Where-Object { $_ -and $_ -ne $PID } | ForEach-Object { cmd /c "taskkill /PID $_ /T /F >nul 2>&1" }
    # the old installer started Core from a Startup shortcut; two launchers would fight over the port
    $oldLnk = Join-Path ([Environment]::GetFolderPath("Startup")) "MAZ Core.lnk"
    if (Test-Path $oldLnk) { Remove-Item $oldLnk -Force -ErrorAction SilentlyContinue }

    $FwDir = Join-Path $Install "fw"
    $code = "import os,sys; os.environ.setdefault('MAZ_FW_DIR', r'$FwDir'); os.environ.setdefault('MAZ_REPO_DIR', r'$Repo'); sys.path.insert(0, r'$Repo\host'); from mazhost.__main__ import main; main()"
    $act = New-ScheduledTaskAction -Execute $VenvPyw -Argument ('-c "' + $code + '"') -WorkingDirectory $Install
    $trg = New-ScheduledTaskTrigger -AtLogOn -User $env:USERNAME
    $set = New-ScheduledTaskSettingsSet -RestartCount 99 -RestartInterval (New-TimeSpan -Minutes 1) `
        -ExecutionTimeLimit ([TimeSpan]::Zero) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable
    Register-ScheduledTask -TaskName $Task -Action $act -Trigger $trg -Settings $set -RunLevel Limited -Force | Out-Null
    Start-ScheduledTask -TaskName $Task

    # ---- 5. firewall: one UAC prompt only if a rule is missing -----------------------------------
    Step "5/7 Letting your Cardputer reach this PC"
    if ((Get-MissingFirewallRules).Count -eq 0) {
        Note "Already allowed"
    } elseif (Test-Admin) {
        New-FirewallRules
        Note "Allowed (home networks only)"
    } else {
        Note "Windows will ask once for permission - click Yes."
        try {
            $self = $MyInvocation.MyCommand.Path
            $p = Start-Process powershell.exe -Verb RunAs -Wait -PassThru -WindowStyle Hidden -ArgumentList `
                "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$self`"", "-FirewallOnly", "-Port", "$Port"
            if ((Get-MissingFirewallRules).Count -eq 0) { Note "Allowed (home networks only)" }
            else { Warn "Not allowed. The Cardputer may not find this PC. Run nod-setup.cmd again and click Yes." }
        } catch {
            Warn "Permission was declined. The Cardputer may not find this PC. Run nod-setup.cmd again and click Yes."
        }
    }

    $health = Wait-Health 90
    if (-not $health) { throw "nod Core did not start. Look at $Install\logs for details, then run nod-setup.cmd again." }

    # ---- 6. firmware + updates through Core's own self-update (no GitHub token needed) -------------
    Step "6/7 Getting the latest Cardputer firmware"
    $token = Read-EnvValue "MAZ_TOKEN"
    $auth = @{ Authorization = "Bearer $token" }
    $restarting = $false
    try {
        $u = Invoke-RestMethod "http://127.0.0.1:$Port/core/update/check" -Method Post -Headers $auth -TimeoutSec 600
        if ("$($u.last_result)" -match "restarting") { $restarting = $true }
        if ($u.last_error) { Warn "Update check: $($u.last_error)" }
        elseif ($u.staged_fw) { Note "Firmware $($u.staged_fw.version) is ready for your Cardputer" }
        else { Note "Update check done: $($u.last_result)" }
    } catch {
        Warn "Could not check for updates now (Core will keep trying by itself)."
    }
    # a code update restarts Core; wait for it to be back
    if ($restarting) { Note "nod Core updated itself and is restarting..."; Start-Sleep -Seconds 20 } else { Start-Sleep -Seconds 2 }
    $health = Wait-Health 120
    if (-not $health) { throw "nod Core did not come back after updating. Look at $Install\logs, then run nod-setup.cmd again." }

    # ---- 7. nod Flow dictation hotkey, only if its libraries really loaded -------------------------
    Step "7/7 Dictation hotkey (nod Flow)"
    $flowOk = $false
    try {
        & $VenvPy -c "import sounddevice, pyperclip, httpx, tkinter" 2>$null
        $flowOk = ($LASTEXITCODE -eq 0)
    } catch { $flowOk = $false }
    if ($flowOk) {
        $lnk = Join-Path ([Environment]::GetFolderPath("Startup")) "nodflow.lnk"
        $s = (New-Object -ComObject WScript.Shell).CreateShortcut($lnk)
        $s.TargetPath = $VenvPyw
        $s.Arguments = "`"$(Join-Path $HostDir 'nodflow.py')`""
        $s.WindowStyle = 7
        $s.Save()
        $running = Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | Where-Object { $_.CommandLine -match "nodflow\.py" }
        if (-not $running) { Start-Process $VenvPyw -ArgumentList "`"$(Join-Path $HostDir 'nodflow.py')`"" -WindowStyle Hidden }
        Note "On: hold Right Ctrl, speak, let go - the text is typed for you"
    } else {
        Warn "Skipped: a microphone library did not load. Everything else works."
    }

    # ---- pairing code (loopback, local key, key never shown) ---------------------------------------
    $pair = Invoke-RestMethod "http://127.0.0.1:$Port/pair/start" -Method Post -Headers $auth -TimeoutSec 15
    $Code = [string]$pair.code
    $token = $null
    $auth = $null
    $Ok = $true
} catch {
    Write-Host ""
    Write-Host "SETUP STOPPED: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Nothing is broken. Fix the line above and double-click nod-setup.cmd again." -ForegroundColor Red
}

if ($Ok) {
    $lan = Get-LanAddress
    Write-Host ""
    Write-Host "  ============================================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "      nod Core is running." -ForegroundColor Green
    Write-Host ""
    Write-Host "      Your pairing code:    $Code" -ForegroundColor Yellow
    Write-Host "      (works once, for 5 minutes - run nod-setup.cmd for a new one)"
    Write-Host ""
    Write-Host "      NEXT: on your Cardputer press Ctrl+U and type the code." -ForegroundColor Green
    Write-Host ""
    Write-Host "  ============================================================" -ForegroundColor Green
    if ($lan) { Write-Host "  This PC: ${lan}:$Port" }
    $ts = Get-Command tailscale -ErrorAction SilentlyContinue
    if ($ts) {
        Write-Host "  Optional, to reach nod away from home: run host\setup-remote.ps1 (no admin needed)."
    }
    Write-Host ""
    try { Stop-Transcript | Out-Null } catch { }
    exit 0
}
try { Stop-Transcript | Out-Null } catch { }
exit 1
