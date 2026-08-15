$ErrorActionPreference = "Stop"
$HostRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$EnvPath = Join-Path $HostRoot ".env"
$Runtime = Join-Path $HostRoot "runtime"
$SwapExe = Join-Path $Runtime "llama-swap\llama-swap.exe"
$SwapConfig = Join-Path $HostRoot "llama-swap.yaml"
$BuildId = (Get-Content (Join-Path $HostRoot "BUILD-ID") -Raw).Trim()
$LogDir = Join-Path $HostRoot "logs"
$SwapPort = 8790
New-Item -ItemType Directory -Force $LogDir | Out-Null

function Test-MazSwap {
    try {
        $models = Invoke-RestMethod -Uri "http://127.0.0.1:$SwapPort/v1/models" -TimeoutSec 2
        return ($null -ne $models.data)
    } catch { return $false }
}

if (-not (Test-Path $SwapExe)) { throw "MAZ-owned llama-swap runtime is missing. Run INSTALL.cmd again." }
if (-not (Test-Path $SwapConfig)) { throw "llama-swap.yaml is missing. Run INSTALL.cmd again." }
$configText = Get-Content $SwapConfig -Raw
if ($configText -notmatch [regex]::Escape("# MAZ_BUILD_ID=$BuildId")) {
    throw "llama-swap.yaml belongs to a different MAZ Core build. Run INSTALL.cmd again."
}

# Never trust a responding /v1/models endpoint until we prove the listener is
# the MAZ-owned llama-swap process. This prevents accidentally routing to an
# unrelated OpenAI-compatible service already using 8790.
$listener = Get-NetTCPConnection -LocalPort $SwapPort -State Listen -ErrorAction SilentlyContinue | Select-Object -First 1
if ($listener) {
    $proc = Get-CimInstance Win32_Process -Filter "ProcessId=$($listener.OwningProcess)" -ErrorAction SilentlyContinue
    $cmd = if ($proc) { [string]$proc.CommandLine } else { "" }
    $owned = $cmd -and $cmd.IndexOf($HostRoot, [System.StringComparison]::OrdinalIgnoreCase) -ge 0 -and $cmd -match 'llama-swap\.exe'
    if ($owned -and (Test-MazSwap)) { exit 0 }
    if ($owned) {
        Stop-Process -Id $listener.OwningProcess -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 750
    } else {
        throw "Port $SwapPort is occupied by a non-MAZ process. It was left untouched."
    }
}

$out = Join-Path $LogDir "llama-swap.out.log"
$err = Join-Path $LogDir "llama-swap.err.log"
Remove-Item $out,$err -Force -ErrorAction SilentlyContinue
$p = Start-Process -FilePath $SwapExe -ArgumentList @("--config", "`"$SwapConfig`"", "--listen", "127.0.0.1:$SwapPort") `
    -WindowStyle Hidden -PassThru -RedirectStandardOutput $out -RedirectStandardError $err

for ($i = 0; $i -lt 240; $i++) {
    Start-Sleep -Milliseconds 250
    if (Test-MazSwap) { exit 0 }
    if ($p.HasExited) { break }
}
$tail = if (Test-Path $err) { (Get-Content $err -Tail 80 -ErrorAction SilentlyContinue) -join "`n" } else { "no stderr log" }
throw "llama-swap did not become ready.`n$tail"
