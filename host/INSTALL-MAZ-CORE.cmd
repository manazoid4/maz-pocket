@echo off
setlocal
cd /d "%~dp0"

rem Compatibility entrypoint. The v0.6.4 recovery installer has one canonical
rem launcher; keeping this filename only prevents old documentation/bookmarks
rem from resurrecting the deprecated nested installer path.
if exist "%~dp0INSTALL.cmd" (
  call "%~dp0INSTALL.cmd"
  exit /b %ERRORLEVEL%
)

echo MAZ Core recovery installer is incomplete.
echo Use the complete v0.6.4 recovery package and run INSTALL.cmd.
pause
exit /b 2
