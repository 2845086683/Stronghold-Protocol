@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Server.ps1" Run -Mode LAN %*
if errorlevel 1 pause
