@echo off
setlocal EnableDelayedExpansion

set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
set "SLNDIR=%~dp0.."
set "SLN=%SLNDIR%\renderdoc.sln"
set "MSYS2_BASH=C:\msys64\usr\bin\bash.exe"
set "SCRIPTDIR=%~dp0"

:: Parse version from version.h
for /f "tokens=3" %%a in ('findstr /C:"RENDERDOC_VERSION_MAJOR" "%SLNDIR%\renderdoc\api\replay\version.h" ^| findstr /V STRINGIZE') do set "VER_MAJOR=%%a"
for /f "tokens=3" %%a in ('findstr /C:"RENDERDOC_VERSION_MINOR" "%SLNDIR%\renderdoc\api\replay\version.h" ^| findstr /V STRINGIZE') do set "VER_MINOR=%%a"
set "VERSION=%VER_MAJOR%.%VER_MINOR%"
set "PKGNAME=RenderSocoDoc_%VERSION%"

echo ============================================================
echo  RenderSocoDoc Full Build + Package  (v%VERSION%)
echo  Win x86 + x64 + Android ARM32 + ARM64
echo ============================================================

:: ---- Build Win x86 ----
echo.
echo [1/4] Building Win x86 Release ...
"%MSBUILD%" "%SLN%" /t:Build /p:Configuration=Release /p:Platform=x86 /m /nologo
if errorlevel 1 (
    echo ERROR: x86 build failed.
    exit /b 1
)
echo x86 build OK.

:: ---- Build Win x64 ----
echo.
echo [2/4] Building Win x64 Release ...
"%MSBUILD%" "%SLN%" /t:Build /p:Configuration=Release /p:Platform=x64 /m /nologo
if errorlevel 1 (
    echo ERROR: x64 build failed.
    exit /b 1
)
echo x64 build OK.

:: ---- Build Android ARM32 ----
echo.
echo [3/4] Building Android ARM32 Release ...
"%MSYS2_BASH%" -l "%SCRIPTDIR%build_android_arm32.sh"
if errorlevel 1 (
    echo ERROR: Android ARM32 build failed.
    exit /b 1
)
echo Android ARM32 build OK.

:: ---- Build Android ARM64 ----
echo.
echo [4/4] Building Android ARM64 Release ...
"%MSYS2_BASH%" -l "%SCRIPTDIR%build_android_arm64.sh"
if errorlevel 1 (
    echo ERROR: Android ARM64 build failed.
    exit /b 1
)
echo Android ARM64 build OK.

:: ---- Package ----
echo.
echo Packaging %PKGNAME% ...

cd /d "%SLNDIR%"
if exist dist rd /s /q dist
if exist package rd /s /q package
mkdir dist\Release64
mkdir dist\Release32

:: Copy x64 outputs
robocopy x64\Release dist\Release64 /E /XD obj pymodules /XF *.lib *.ipdb *.iobj >nul

:: Copy x86 outputs
robocopy Win32\Release dist\Release32 /E /XD obj pymodules /XF *.lib *.ipdb *.iobj >nul

:: Copy d3dcompiler from Windows SDK
set "WINSDK=C:\Program Files (x86)\Windows Kits\10\Redist\D3D"
if exist "%WINSDK%\x64\d3dcompiler_47.dll" copy /y "%WINSDK%\x64\d3dcompiler_47.dll" dist\Release64\ >nul
if exist "%WINSDK%\x86\d3dcompiler_47.dll" copy /y "%WINSDK%\x86\d3dcompiler_47.dll" dist\Release32\ >nul

:: Copy LICENSE
if exist LICENSE.md (
    copy /y LICENSE.md dist\Release64\ >nul
    copy /y LICENSE.md dist\Release32\ >nul
)

:: Copy Android APKs
mkdir dist\Release64\plugins\android
if exist build-android-arm32\bin\org.renderdoc.renderdoccmd.arm32.apk (
    copy /y build-android-arm32\bin\org.renderdoc.renderdoccmd.arm32.apk dist\Release64\plugins\android\ >nul
)
if exist build-android-arm64\bin\org.renderdoc.renderdoccmd.arm64.apk (
    copy /y build-android-arm64\bin\org.renderdoc.renderdoccmd.arm64.apk dist\Release64\plugins\android\ >nul
)

:: Bundle x86 core files into x64 package
mkdir dist\Release64\x86
for %%f in (d3dcompiler_47.dll renderdoc.dll renderdoc.json render_soco_docshim32.dll render_soco_doccmd.exe system_load.dll system_load.json dbghelp.dll symsrv.dll symsrv.yes) do (
    if exist "dist\Release32\%%f" copy /y "dist\Release32\%%f" dist\Release64\x86\ >nul
)

:: Remove PDBs and build artifacts
del /s /q dist\Release64\*.pdb >nul 2>&1
del /q dist\Release64\*.exp dist\Release64\*.lib dist\Release64\*.metagen dist\Release64\*.xml >nul 2>&1
del /q dist\Release64\*.vshost.* >nul 2>&1

:: Create zip
mkdir package
xcopy /e /i /q dist\Release64 "package\%PKGNAME%_64"
powershell -Command "Compress-Archive -Path 'package\%PKGNAME%_64' -DestinationPath 'package\%PKGNAME%_64.zip' -Force"

echo.
echo ============================================================
echo  Done!  package\%PKGNAME%_64.zip
echo ============================================================
