param(
    [Parameter(Position=0)]
    [string]$Text = ""
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$EnvPath = Join-Path $Root ".env"

function Get-MazEnv([string]$Name) {
    if (-not (Test-Path $EnvPath)) { return "" }
    $escaped = [regex]::Escape($Name)
    $match = Select-String -Path $EnvPath -Pattern "^$escaped=(.*)$" | Select-Object -First 1
    if ($match -and $match.Matches.Count) { return $match.Matches[0].Groups[1].Value.Trim() }
    return ""
}

if (-not $Text) {
    try { $Text = Get-Clipboard -Raw } catch { $Text = "" }
}
$Text = ($Text -replace "`0", "").Trim()
if (-not $Text) { throw "Nothing to Beam. Copy text/a URL first, or pass text to beam.ps1." }
if ($Text.Length -gt 2000) { $Text = $Text.Substring(0, 2000) }

$Token = Get-MazEnv "MAZ_TOKEN"
if (-not $Token) { throw "MAZ Core token is not configured. Run INSTALL-MAZ-CORE.cmd first." }
$Port = Get-MazEnv "MAZ_PORT"
if (-not $Port) { $Port = "8787" }

$Headers = @{ Authorization = "Bearer $Token" }
$Body = @{ text = $Text } | ConvertTo-Json -Compress
$result = Invoke-RestMethod -Uri "http://127.0.0.1:$Port/beam/to-pocket" -Method Post -Headers $Headers -ContentType "application/json" -Body $Body -TimeoutSec 5
if (-not $result.ok) { throw "MAZ Core did not accept the Beam." }
Write-Host "BEAM QUEUED -> MAZ Pocket" -ForegroundColor Green
Write-Host $Text
