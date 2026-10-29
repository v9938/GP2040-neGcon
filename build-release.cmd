@echo off
setlocal
where pwsh >nul 2>nul
if not errorlevel 1 (
    pwsh -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-release.ps1" %*
) else (
    powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-release.ps1" %*
)
exit /b %errorlevel%
