@echo off
setlocal enabledelayedexpansion
REM ============================================================
REM  RenderSocoDoc Windows Build Script
REM  Usage:
REM    build_win.bat              - Incremental build x64+x86
REM    build_win.bat rebuild      - Full rebuild x64+x86
REM    build_win.bat x64          - Incremental build x64 only
REM    build_win.bat x86          - Incremental build x86 only
REM    build_win.bat rebuild x64  - Full rebuild x64 only
REM ============================================================

set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
set "SLN=D:\Project\CPP\renderdoc\renderdoc.sln"
set "BUILD_TARGET=Build"
set "BUILD_X64=1"
set "BUILD_X86=1"

REM Parse arguments
:parse_args
if "%~1"=="" goto :start_build
if /i "%~1"=="rebuild" (
    set "BUILD_TARGET=Rebuild"
    shift
    goto :parse_args
)
if /i "%~1"=="x64" (
    set "BUILD_X86=0"
    shift
    goto :parse_args
)
if /i "%~1"=="x86" (
    set "BUILD_X64=0"
    shift
    goto :parse_args
)
echo Unknown argument: %~1
exit /b 1

:start_build
echo ============================================================
echo  Build Target: %BUILD_TARGET%
echo  x64: %BUILD_X64%  x86: %BUILD_X86%
echo ============================================================

set "FAIL=0"

if "%BUILD_X64%"=="1" (
    echo.
    echo [x64 Release] Starting %BUILD_TARGET%...
    "%MSBUILD%" "%SLN%" /t:%BUILD_TARGET% /p:Configuration=Release /p:Platform=x64 /m /nologo
    if errorlevel 1 (
        echo [x64 Release] FAILED!
        set "FAIL=1"
    ) else (
        echo [x64 Release] OK
    )
)

if "%BUILD_X86%"=="1" (
    echo.
    echo [x86 Release] Starting %BUILD_TARGET%...
    "%MSBUILD%" "%SLN%" /t:%BUILD_TARGET% /p:Configuration=Release /p:Platform=x86 /m /nologo
    if errorlevel 1 (
        echo [x86 Release] FAILED!
        set "FAIL=1"
    ) else (
        echo [x86 Release] OK
    )
)

echo.
if "%FAIL%"=="1" (
    echo ============ BUILD FAILED ============
    exit /b 1
) else (
    echo ============ BUILD SUCCESS ============
)
exit /b 0
