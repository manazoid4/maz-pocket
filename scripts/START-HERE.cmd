@echo off
setlocal
cd /d "%~dp0"

rem Prefer PowerShell 7 when the machine has it, but a normal Windows box only
rem ships Windows PowerShell 5.1, so that path must keep working. setup-all.ps1
rem is therefore held to a 5.1-safe subset (ASCII only) and is packaged with a
rem UTF-8 BOM so 5.1 never falls back to the ANSI code page.
rem MAZ_FORCE_WINDOWS_POWERSHELL exists so CI can prove the 5.1 fallback still
rem works on a runner that happens to have pwsh installed. Users never set it.
set "MAZPS=powershell.exe"
if not defined MAZ_FORCE_WINDOWS_POWERSHELL where pwsh.exe >nul 2>&1 && set "MAZPS=pwsh.exe"

rem Arguments are passed through so CI can exercise this exact entry point in a
rem non-destructive mode (-SkipCore -SkipSd -NoBrowser). Users pass nothing.
"%MAZPS%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup-all.ps1" %*
set "ERR=%ERRORLEVEL%"
echo.
if not "%ERR%"=="0" echo MAZ Pocket setup stopped with error %ERR%.
pause
exit /b %ERR%
