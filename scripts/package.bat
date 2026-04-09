@echo off
setlocal enabledelayedexpansion
REM ============================================================
REM  RenderSocoDoc Package Script
REM  Usage:
REM    package.bat              - Package Windows only
REM    package.bat android      - Package Windows + Android APKs
REM ============================================================

set "ROOT=D:\Project\CPP\renderdoc"
set "INCLUDE_ANDROID=0"

if /i "%~1"=="android" set "INCLUDE_ANDROID=1"

cd /d "%ROOT%"

REM Extract version
for /f "tokens=3" %%a in ('findstr /r "RENDERDOC_VERSION_MAJOR" renderdoc\api\replay\version.h') do set "VMAJOR=%%a"
for /f "tokens=3" %%a in ('findstr /r "RENDERDOC_VERSION_MINOR" renderdoc\api\replay\version.h') do set "VMINOR=%%a"
set "VERSION=%VMAJOR%.%VMINOR%"
set "PKGNAME=RenderSocoDoc_%VERSION%"

echo ============================================================
echo  Packaging %PKGNAME%
echo  Include Android: %INCLUDE_ANDROID%
echo ============================================================

REM Verify build outputs exist
if not exist "x64\Release\renderdoc.dll" (
    echo ERROR: x64\Release\renderdoc.dll not found. Run build_win.bat first.
    exit /b 1
)
if not exist "Win32\Release\renderdoc.dll" (
    echo ERROR: Win32\Release\renderdoc.dll not found. Run build_win.bat first.
    exit /b 1
)

if "%INCLUDE_ANDROID%"=="1" (
    if not exist "build-android-arm32\bin\org.renderdoc.renderdoccmd.arm32.apk" (
        echo ERROR: ARM32 APK not found. Run build_android.bat first.
        exit /b 1
    )
    if not exist "build-android-arm64\bin\org.renderdoc.renderdoccmd.arm64.apk" (
        echo ERROR: ARM64 APK not found. Run build_android.bat first.
        exit /b 1
    )
)

REM Clean previous
if exist dist rmdir /s /q dist
if exist package rmdir /s /q package
mkdir dist\Release64
mkdir dist\Release32

echo [1/6] Copying x64 build outputs...
robocopy "x64\Release" "dist\Release64" /e /xd obj pymodules /xf *.lib *.ipdb *.iobj /njh /njs /ndl /nc /ns >nul

echo [2/6] Copying x86 build outputs...
robocopy "Win32\Release" "dist\Release32" /e /xd obj pymodules /xf *.lib *.ipdb *.iobj /njh /njs /ndl /nc /ns >nul

echo [3/6] Copying d3dcompiler and LICENSE...
copy /y "C:\Program Files (x86)\Windows Kits\10\Redist\D3D\x64\d3dcompiler_47.dll" "dist\Release64\" >nul
copy /y "C:\Program Files (x86)\Windows Kits\10\Redist\D3D\x86\d3dcompiler_47.dll" "dist\Release32\" >nul
copy /y LICENSE.md "dist\Release64\" >nul

echo [4/6] Bundling x86 into x64 package...
mkdir "dist\Release64\x86"
for %%f in (d3dcompiler_47.dll renderdoc.dll renderdoc.json render_soco_docshim32.dll render_soco_doccmd.exe system_load.dll system_load.json dbghelp.dll symsrv.dll symsrv.yes) do (
    if exist "dist\Release32\%%f" copy /y "dist\Release32\%%f" "dist\Release64\x86\" >nul
)

if "%INCLUDE_ANDROID%"=="1" (
    echo [4.5/6] Copying Android APKs...
    mkdir "dist\Release64\plugins\android"
    copy /y "build-android-arm32\bin\org.renderdoc.renderdoccmd.arm32.apk" "dist\Release64\plugins\android\" >nul
    copy /y "build-android-arm64\bin\org.renderdoc.renderdoccmd.arm64.apk" "dist\Release64\plugins\android\" >nul
)

echo [5/6] Cleaning build artifacts...
del /s /q "dist\Release64\*.pdb" >nul 2>nul
del /q "dist\Release64\*.exp" >nul 2>nul
del /q "dist\Release64\*.lib" >nul 2>nul
del /q "dist\Release64\*.metagen" >nul 2>nul
del /q "dist\Release64\*.xml" >nul 2>nul

echo [6/6] Creating zip...
mkdir package
xcopy /e /i /q "dist\Release64" "package\%PKGNAME%_64" >nul
powershell -Command "Compress-Archive -Path 'package\%PKGNAME%_64' -DestinationPath 'package\%PKGNAME%_64.zip' -Force"

echo.
echo ============================================================
echo  Done: package\%PKGNAME%_64.zip
echo ============================================================
for %%A in ("package\%PKGNAME%_64.zip") do echo  Size: %%~zA bytes

exit /b 0
