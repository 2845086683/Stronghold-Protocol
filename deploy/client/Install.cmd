@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Deploy-Client.ps1" -Action Install %*
set "taskResult=%ERRORLEVEL%"
if not "%taskResult%"=="0" echo Installation failed. Read the error above; no official game updater is started.
pause
exit /b %taskResult%
