$ErrorActionPreference = "Stop"
$HostRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Venv = Join-Path $HostRoot ".venv"
$Python = Join-Path $Venv "Scripts\python.exe"
$Version = (Get-Content (Join-Path $HostRoot "CORE_VERSION") -Raw).Trim()

function Invoke-NativeLive([string]$Exe, [string[]]$Args) {
    $old = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $captured = @(& $Exe @Args 2>&1)
        $code = $LASTEXITCODE
        foreach ($line in $captured) { Write-Host ([string]$line) }
        return [int]$code
    } finally {
        $ErrorActionPreference = $old
    }
}

function New-MazVenv {
    Remove-Item $Venv -Recurse -Force -ErrorAction SilentlyContinue
    $Py = Get-Command py -ErrorAction SilentlyContinue
    $PythonCmd = Get-Command python -ErrorAction SilentlyContinue
    if ($Py) {
        $code = Invoke-NativeLive $Py.Source @("-3","-m","venv",$Venv)
    } elseif ($PythonCmd) {
        $code = Invoke-NativeLive $PythonCmd.Source @("-m","venv",$Venv)
    } else {
        throw "Python 3.11+ is required. Install Python once and run INSTALL.cmd again."
    }
    if ($code -ne 0 -or -not (Test-Path $Python)) { throw "Could not create the MAZ Core Python virtual environment." }
}

if (-not (Test-Path $Python)) { New-MazVenv }

# Reject a stale venv created by an unsupported Python and rebuild once.
$code = Invoke-NativeLive $Python @("-c","import sys; raise SystemExit(0 if sys.version_info >= (3,11) else 3)")
if ($code -ne 0) { New-MazVenv }

function Install-And-SmokeTest {
    $pip = Invoke-NativeLive $Python @("-m","pip","install","--disable-pip-version-check","--quiet","-r",(Join-Path $HostRoot "requirements.txt"))
    if ($pip -ne 0) { return $false }
    $smoke = Invoke-NativeLive $Python @("-c","import fastapi,uvicorn,httpx,serial,faster_whisper; import mazhost.config,mazhost.llm,mazhost.app")
    return ($smoke -eq 0)
}

if (-not (Install-And-SmokeTest)) {
    Write-Host "[WARN] Existing Python environment was unhealthy; rebuilding it once." -ForegroundColor Yellow
    New-MazVenv
    if (-not (Install-And-SmokeTest)) { throw "MAZ Core Python environment could not pass dependency/import verification after a clean rebuild." }
}

$EnvPath = Join-Path $HostRoot ".env"
if (-not (Test-Path $EnvPath)) {
    $Template = Get-Content (Join-Path $HostRoot ".env.example") -Raw
    $old = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try { $Token = & $Python -c "import secrets; print(secrets.token_urlsafe(18))"; $code=$LASTEXITCODE }
    finally { $ErrorActionPreference=$old }
    if ($code -ne 0 -or -not $Token) { throw "Could not generate MAZ pairing token." }
    $Template.Replace("change-me-before-first-run", $Token) | Set-Content $EnvPath -NoNewline -Encoding utf8
}

Write-Host "[PASS] Python environment - MAZ Core v$Version" -ForegroundColor Green
