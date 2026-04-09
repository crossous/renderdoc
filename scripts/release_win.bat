@echo off
REM ============================================================
REM  RenderSocoDoc One-Click Release (Windows Only)
REM  Incremental build + package, no Android
REM ============================================================

echo ============ Windows Release (No Android) ============
echo.

call "%~dp0build_win.bat"
if errorlevel 1 (
    echo Build failed, aborting package.
    pause
    exit /b 1
)

echo.
call "%~dp0package.bat"
if errorlevel 1 (
    echo Package failed.
    pause
    exit /b 1
)

echo.
echo ============ RELEASE COMPLETE ============
pause
