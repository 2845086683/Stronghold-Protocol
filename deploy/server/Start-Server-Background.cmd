@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Server.ps1" Start -Mode LAN %*
if errorlevel 1 pause
