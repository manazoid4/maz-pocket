param(
    [switch]$SkipOllamaInstall
)

$ErrorActionPreference = "Stop"
$HostRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$LiteModel = "maz-pocket-lite:latest"
$LiteBase = "qwen3.5:4b-q4_K_M"
$Modelfile = Join-Path $HostRoot "models\Modelfile.maz-pocket-lite"

function Find-Ollama {
    return Get-Command ollama -ErrorAction SilentlyContinue
}

$Ollama = Find-Ollama
if (-not $Ollama -and -not $SkipOllamaInstall) {
    $Winget = Get-Command winget -ErrorAction SilentlyContinue
    if ($Winget) {
        Write-Host "Ollama not found. Installing it once..."
        & winget install --id Ollama.Ollama -e --silent --accept-package-agreements --accept-source-agreements
        $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
        $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
        $env:Path = "$machinePath;$userPath"
        $Ollama = Find-Ollama
    }
}

if (-not $Ollama) {
    Write-Warning "Ollama is not installed. MAZ Core will still run; local AI stays unavailable until Ollama is installed."
    exit 0
}

# Ensure the local API exists. The desktop Ollama app may already own it; only
# launch `ollama serve` when there is no listener.
try {
    Invoke-RestMethod -Uri "http://127.0.0.1:11434/api/tags" -TimeoutSec 2 | Out-Null
} catch {
    Start-Process -FilePath $Ollama.Source -ArgumentList "serve" -WindowStyle Hidden
    Start-Sleep -Seconds 2
}

$Installed = @()
try {
    $Tags = Invoke-RestMethod -Uri "http://127.0.0.1:11434/api/tags" -TimeoutSec 5
    $Installed = @($Tags.models | ForEach-Object { if ($_.name) { $_.name } else { $_.model } })
} catch { }

if ($Installed -notcontains $LiteModel) {
    Write-Host "Installing MAZ Pocket Lite (~3.4 GB download, lower VRAM fallback)..."
    if ($Installed -notcontains $LiteBase) {
        & ollama pull $LiteBase
        if ($LASTEXITCODE -ne 0) { throw "Could not pull $LiteBase" }
    }
    & ollama create $LiteModel -f $Modelfile
    if ($LASTEXITCODE -ne 0) { throw "Could not create $LiteModel" }
} else {
    Write-Host "MAZ Pocket Lite already installed."
}

Write-Host "Local model chain ready:"
Write-Host "  Primary: lfm2.5-8b-a1b-gpu:latest (kept if installed)"
Write-Host "  Backup:  $LiteModel"
Write-Host "  Policy:  primary -> backup; cloud only on AUTO route"
