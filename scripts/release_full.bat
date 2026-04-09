@echo off
REM ============================================================
REM  RenderSocoDoc One-Click Full Release (Windows + Android)
REM ============================================================

echo ============ Full Release (Windows + Android) ============
echo.

call "%~dp0build_win.bat"
if errorlevel 1 (
    echo Windows build failed, aborting.
    pause
    exit /b 1
)

echo.
call "%~dp0build_android.bat"
if errorlevel 1 (
    echo Android build failed, aborting.
    pause
    exit /b 1
)

echo.
call "%~dp0package.bat" android
if errorlevel 1 (
    echo Package failed.
    pause
    exit /b 1
)

echo.
echo ============ FULL RELEASE COMPLETE ============
pause
