@echo off
rem One-time bootstrap for Core auto-update. Run on the PC: BOOTSTRAP-AUTOUPDATE.cmd [deploy-worktree-path]
rem Default worktree: C:\Users\manaz\maz-pocket-deploy
setlocal
set "WT=%~1"
if "%WT%"=="" set "WT=C:\Users\manaz\maz-pocket-deploy"
set "INSTALL=%LOCALAPPDATA%\MAZ Core"
set "PY=%INSTALL%\.venv\Scripts\python.exe"
if not exist "%WT%\.git" ( echo Not a git checkout: %WT% & exit /b 1 )
if not exist "%PY%" ( echo No venv python at %PY% - run host\setup.ps1 in "%INSTALL%" first & exit /b 1 )

echo [1/5] Updating deploy checkout %WT%
git -C "%WT%" fetch origin deploy/local || exit /b 1
git -C "%WT%" checkout deploy/local || exit /b 1
git -C "%WT%" pull --ff-only origin deploy/local || exit /b 1

echo [2/5] Installing requirements with the Core python
"%PY%" -m pip install -r "%WT%\host\requirements.txt" || exit /b 1

echo [3/5] Pointing the "nod Core" task at the checkout (keeps .env, logs, fw in "%INSTALL%")
powershell -NoProfile -ExecutionPolicy Bypass -File "%WT%\host\point-core-task-at-checkout.ps1" -Repo "%WT%" -Install "%INSTALL%" || exit /b 1

echo [4/5] Restarting the task
schtasks /End /TN "nod Core" >nul 2>&1
timeout /t 3 /nobreak >nul
schtasks /Run /TN "nod Core" || exit /b 1

echo [5/5] Waiting for /health
timeout /t 12 /nobreak >nul
for /f "tokens=1,* delims==" %%A in ('findstr /b "MAZ_TOKEN=" "%INSTALL%\.env"') do set "TOK=%%B"
curl -s -H "Authorization: Bearer %TOK%" http://127.0.0.1:8787/health
echo.
echo Done. Check git_sha and update in the output above. Logs: %INSTALL%\logs\core.log
endlocal
