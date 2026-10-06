# Pull the latest repo, rebuild nothing, reinstall Core + stage the newest firmware. U4 will fetch release assets instead.
param([string]$Fw)
$ErrorActionPreference = "Stop"
$Repo = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (Test-Path (Join-Path $Repo ".git")) { git -C $Repo pull --ff-only }
& (Join-Path $Repo "host\install-core-task.ps1") @(if ($Fw) { "-Fw"; $Fw })
