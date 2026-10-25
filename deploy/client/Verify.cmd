@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Deploy-Client.ps1" -Action Verify %*
set "taskResult=%ERRORLEVEL%"
pause
exit /b %taskResult%
