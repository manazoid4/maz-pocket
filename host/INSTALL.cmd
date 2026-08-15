@echo off
setlocal
cd /d "%~dp0"
title MAZ CORE v0.6.4 RECOVERY
cls
echo ============================================================
echo  MAZ CORE v0.6.4 - RECOVERY / LLAMA-SWAP
echo  BUILD: MAZCORE-064-RECOVERY-20260815C
echo ============================================================
echo.
echo  This installer does NOT use winget and does NOT reflash Pocket.
echo  It verifies the local AI path end-to-end before reporting READY.
echo.
echo  RUNNING FROM:
echo  %~dp0
echo.
if not exist "%~dp0install-core.ps1" (
  echo ERROR: install-core.ps1 is missing.
  echo Extract the entire ZIP first, then run this file.
  pause
  exit /b 2
)
echo %~dp0 | findstr /I /C:"\AppData\Local\Temp\" >nul
if not errorlevel 1 (
  echo ERROR: This looks like Windows' temporary ZIP folder.
  echo Right-click the ZIP, choose Extract All, then run INSTALL.cmd there.
  pause
  exit /b 3
)
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-core.ps1"
set "ERR=%ERRORLEVEL%"
echo.
if not "%ERR%"=="0" (
  echo INSTALL STOPPED - error %ERR%.
  echo A sanitized failure report should be on your Desktop as MAZ-Core-FAILED.txt
) else (
  echo MAZ Core v0.6.4 verified and ready.
)
echo.
pause
exit /b %ERR%
