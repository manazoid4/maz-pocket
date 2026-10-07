@echo off
rem nod one-click setup. Double-click this file. Safe to run again any time.
setlocal
title nod setup
cd /d "%~dp0"
if exist "%~dp0nod-setup.ps1" (
  powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0nod-setup.ps1" %*
) else (
  rem Downloaded on its own: fetch the real installer once, it clones the repo itself.
  powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; $d=Join-Path $env:TEMP 'nod-setup.ps1'; Invoke-WebRequest -UseBasicParsing 'https://raw.githubusercontent.com/manazoid4/maz-pocket/deploy/local/host/nod-setup.ps1' -OutFile $d; & $d; exit $LASTEXITCODE"
)
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" echo nod setup did not finish (code %RC%). Read the message above, fix it, then double-click nod-setup.cmd again.
pause
exit /b %RC%
