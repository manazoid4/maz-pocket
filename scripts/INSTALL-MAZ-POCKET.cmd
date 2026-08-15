@echo off
setlocal
cd /d "%~dp0"
echo MAZ Pocket v0.5.1 - safe microSD installer
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-to-sd.ps1"
set "exitcode=%ERRORLEVEL%"
echo.
if not "%exitcode%"=="0" (
  echo Install preparation stopped with error %exitcode%.
) else (
  echo microSD preparation finished successfully.
)
echo.
pause
exit /b %exitcode%
