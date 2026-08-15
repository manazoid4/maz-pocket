@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup-all.ps1"
set "ERR=%ERRORLEVEL%"
echo.
if not "%ERR%"=="0" echo MAZ Pocket setup stopped with error %ERR%.
pause
exit /b %ERR%
