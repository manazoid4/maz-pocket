# Start the local llama.cpp server that MAZ Core talks to when
# MAZ_LOCAL_ENGINE=llamacpp. MAZ Core never launches inference itself: one
# long-lived llama-server keeps the model resident, which is the whole reason
# a second Pocket turn answers in about a second instead of reloading weights.
#
# Paths come from the environment so this script carries no machine-specific
# state. Set them once in host\.env or pass them here.
[CmdletBinding()]
param(
  [string]$Server = $env:MAZ_LLAMACPP_SERVER_EXE,
  [string]$Model  = $env:MAZ_LLAMACPP_MODEL_PATH,
  [int]$Port      = 8080,
  [int]$Context   = 8192,
  [int]$GpuLayers = 99,
  [int]$Threads   = 6
)

$ErrorActionPreference = "Stop"

if (-not $Server) { throw "Set MAZ_LLAMACPP_SERVER_EXE or pass -Server (path to llama-server.exe)." }
if (-not $Model)  { throw "Set MAZ_LLAMACPP_MODEL_PATH or pass -Model (path to a .gguf)." }
if (-not (Test-Path $Server)) { throw "llama-server not found: $Server" }
if (-not (Test-Path $Model))  { throw "Model not found: $Model" }

$existing = Get-NetTCPConnection -State Listen -ErrorAction SilentlyContinue |
  Where-Object { $_.LocalPort -eq $Port }
if ($existing) {
  Write-Host "llama-server already listening on port $Port."
  exit 0
}

# --flash-attn and a quantised KV cache are what let a 6 GB laptop GPU hold an
# 8K context. llama-server clamps the request rather than failing when the
# model does not fit, and /props then reports the context it actually fitted.
$arguments = @(
  "--model", $Model,
  "--host", "127.0.0.1",
  "--port", $Port,
  "--ctx-size", $Context,
  "--n-gpu-layers", $GpuLayers,
  "--threads", $Threads,
  "--flash-attn", "auto",
  "--cache-type-k", "q8_0",
  "--cache-type-v", "q8_0",
  # Pocket answers are capped at 160 tokens. A reasoning model would spend the
  # whole budget thinking and return empty content, so thinking is off at the
  # server as well as in the MAZ Core request.
  "--reasoning-budget", "0",
  # Loopback only. Authenticated MAZ Core stays the single LAN-facing gateway.
  "--no-webui"
)

Write-Host "Starting llama-server on 127.0.0.1:$Port with $(Split-Path $Model -Leaf)..."
Start-Process -FilePath $Server -ArgumentList $arguments -WindowStyle Minimized

$deadline = (Get-Date).AddSeconds(180)
while ((Get-Date) -lt $deadline) {
  try {
    Invoke-RestMethod "http://127.0.0.1:$Port/health" -TimeoutSec 3 | Out-Null
    Write-Host "llama-server healthy on http://127.0.0.1:$Port"
    exit 0
  } catch {
    Start-Sleep -Seconds 2
  }
}

throw "llama-server did not report healthy within 180s. Check the console window it opened."
