@echo off
setlocal
cd /d "%~dp0"
echo MAZ Core v0.5.1 - local PC companion installer
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-core.ps1"
set "exitcode=%ERRORLEVEL%"
echo.
if not "%exitcode%"=="0" (
  echo MAZ Core setup stopped with error %exitcode%.
  echo If Windows blocked Python or the firewall rule, review the message above.
) else (
  echo MAZ Core setup finished successfully.
)
echo.
pause
exit /b %exitcode%
