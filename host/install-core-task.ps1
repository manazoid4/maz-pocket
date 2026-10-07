# Obsolete: replaced by nod-setup.cmd (one installer for everything). Forwarding there.
Write-Host "This script was replaced. Running nod-setup instead (safe to re-run any time)."
& (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "nod-setup.ps1")
exit $LASTEXITCODE
