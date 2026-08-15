@echo off
setlocal
cd /d "%~dp0"
echo MAZ Core - local PC companion installer
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-core.ps1"
set "exitcode=%ERRORLEVEL%"
echo.
if not "%exitcode%"=="0" (
  echo MAZ Core setup stopped with error %exitcode%.
  echo Review the message above for Python, firewall or pairing details.
) else (
  echo MAZ Core setup finished successfully.
)
echo.
pause
exit /b %exitcode%
