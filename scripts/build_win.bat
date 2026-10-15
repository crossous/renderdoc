@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_win.ps1" %*
exit /b %errorlevel%
