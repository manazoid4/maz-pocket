# Post a fake Claude Code approval to Core so you can test the Cardputer overlay.
# Usage: .\host\hooks\test-approval.ps1 [-Summary "cmd"] [-Tool Bash] [-Project maz-pocket]
# Token: $env:MAZ_TOKEN, else MAZ_TOKEN= in host\.env (never printed). Core: $env:MAZ_CORE_URL or http://127.0.0.1:8787
param([string]$Summary = "rm -rf node_modules && npm install --no-audit", [string]$Tool = "Bash", [string]$Project = "maz-pocket")
$ErrorActionPreference = "Stop"
$base = if ($env:MAZ_CORE_URL) { $env:MAZ_CORE_URL.TrimEnd('/') } else { "http://127.0.0.1:8787" }
$token = $env:MAZ_TOKEN
if (-not $token) {
  foreach ($f in @((Join-Path $PSScriptRoot "..\.env"), ".env")) {
    if (Test-Path $f) {
      $line = Get-Content $f | Where-Object { $_ -match '^MAZ_TOKEN=' } | Select-Object -First 1
      if ($line) { $token = ($line -replace '^MAZ_TOKEN=', '').Trim().Trim('"').Trim("'"); if ($token) { break } }
    }
  }
}
if (-not $token) { Write-Error "No MAZ_TOKEN (env or host\.env)"; exit 2 }
$h = @{ Authorization = "Bearer $token" }
$body = @{ tool = $Tool; summary = $Summary; project = $Project; session_id = "test-approval" } | ConvertTo-Json -Compress
$req = Invoke-RestMethod -Method Post -Uri "$base/buddy/request" -Headers $h -ContentType "application/json" -Body $body
Write-Host "Request $($req.id) sent. Press Y/ENTER, N/ESC or A on the Cardputer (60 s)..."
for ($i = 0; $i -lt 15; $i++) {
  $s = Invoke-RestMethod -Uri "$base/buddy/state?id=$($req.id)&wait=4" -Headers $h
  if ($s.decision) { Write-Host "Decision: $($s.decision)"; exit 0 }
}
try { Invoke-RestMethod -Method Post -Uri "$base/buddy/decide" -Headers $h -ContentType "application/json" -Body (@{ id = $req.id; decision = "cancel" } | ConvertTo-Json -Compress) | Out-Null } catch {}
Write-Host "Timed out (no decision); cancelled."
