# Start llama-swap in front of llama-server for MAZ Core's local brain when
# MAZ_LOCAL_ENGINE=llamacpp. llama-swap keeps one process listening on a
# fixed port and loads/unloads models on demand, so MAZ_LLAMACPP_MODEL and
# MAZ_LLAMACPP_BACKUP_MODEL can both live behind the same URL without two
# llama-server processes fighting over the same GPU.
#
# Paths come from the environment so this script carries no machine-specific
# state. Set them once in host\.env or pass them here.
[CmdletBinding()]
param(
  [string]$LlamaSwap = $env:MAZ_LLAMASWAP_EXE,
  [string]$Config    = $env:MAZ_LLAMASWAP_CONFIG,
  [int]$Port         = 8080,
  [string]$WarmModel = ""
)

$ErrorActionPreference = "Stop"

if (-not $LlamaSwap) { throw "Set MAZ_LLAMASWAP_EXE or pass -LlamaSwap (path to llama-swap.exe)." }
if (-not $Config)    { throw "Set MAZ_LLAMASWAP_CONFIG or pass -Config (path to llama-swap's config.yaml)." }
if (-not (Test-Path $LlamaSwap)) { throw "llama-swap not found: $LlamaSwap" }
if (-not (Test-Path $Config))    { throw "llama-swap config not found: $Config" }

$existing = Get-NetTCPConnection -State Listen -ErrorAction SilentlyContinue |
  Where-Object { $_.LocalPort -eq $Port }
if (-not $existing) {
  Write-Host "Starting llama-swap on :$Port with $Config..."
  Start-Process -FilePath $LlamaSwap -ArgumentList @("-config", $Config, "-listen", ":$Port") -WindowStyle Minimized

  $deadline = (Get-Date).AddSeconds(60)
  while ((Get-Date) -lt $deadline) {
    try {
      Invoke-RestMethod "http://127.0.0.1:$Port/health" -TimeoutSec 3 | Out-Null
      break
    } catch {
      Start-Sleep -Seconds 2
    }
  }
} else {
  Write-Host "llama-swap already listening on port $Port."
}

if ($WarmModel) {
  Write-Host "Warming $WarmModel (first request loads it into VRAM, can take a minute)..."
  $body = @{ model = $WarmModel; messages = @(@{ role = "user"; content = "hi" }); max_tokens = 8 } | ConvertTo-Json -Depth 5
  Invoke-RestMethod "http://127.0.0.1:$Port/v1/chat/completions" -Method Post -Body $body -ContentType "application/json" -TimeoutSec 120 | Out-Null
  Write-Host "$WarmModel is loaded and ready."
}
