@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Server.ps1" Backup
pause
