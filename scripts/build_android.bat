@echo off
setlocal enabledelayedexpansion
REM ============================================================
REM  RenderSocoDoc Android Build Script
REM  Must run from MSYS2 environment.
REM  Usage:
REM    build_android.bat            - Build both ARM32 + ARM64
REM    build_android.bat arm32      - Build ARM32 only
REM    build_android.bat arm64      - Build ARM64 only
REM ============================================================

set "MSYS2_BASH=C:\msys64\usr\bin\bash.exe"
set "ROOT=D:/Project/CPP/renderdoc"
set "BUILD_ARM32=1"
set "BUILD_ARM64=1"

if /i "%~1"=="arm32" set "BUILD_ARM64=0"
if /i "%~1"=="arm64" set "BUILD_ARM32=0"

echo ============================================================
echo  Android Build
echo  ARM32: %BUILD_ARM32%  ARM64: %BUILD_ARM64%
echo ============================================================

set "FAIL=0"

if "%BUILD_ARM32%"=="1" (
    echo.
    echo [ARM32] Starting build...
    "%MSYS2_BASH%" -l -c "export JAVA_HOME='/c/Program Files/Java/jdk8u422-b05' && export PATH=\"$JAVA_HOME/bin:$PATH\" && export ANDROID_HOME='D:/Android/Sdk' && export ANDROID_NDK_HOME='D:/Android/Sdk/ndk/android-ndk-r14b' && cd '%ROOT%' && rm -rf build-android-arm32 && mkdir build-android-arm32 && cd build-android-arm32 && cmake -G 'MSYS Makefiles' -DCMAKE_TOOLCHAIN_FILE=\"$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake\" -DCMAKE_ANDROID_STL_TYPE=c++_static -DBUILD_ANDROID=1 -DANDROID_ABI=armeabi-v7a -DANDROID_STL=c++_static -DANDROID_TOOLCHAIN=clang -DCMAKE_BUILD_TYPE=Release -DSTRIP_ANDROID_LIBRARY=On .. && make -j$(nproc)"
    if errorlevel 1 (
        echo [ARM32] FAILED!
        set "FAIL=1"
    ) else (
        echo [ARM32] OK
    )
)

if "%BUILD_ARM64%"=="1" (
    echo.
    echo [ARM64] Starting build...
    "%MSYS2_BASH%" -l -c "export JAVA_HOME='/c/Program Files/Java/jdk8u422-b05' && export PATH=\"$JAVA_HOME/bin:$PATH\" && export ANDROID_HOME='D:/Android/Sdk' && export ANDROID_NDK_HOME='D:/Android/Sdk/ndk/android-ndk-r14b' && cd '%ROOT%' && rm -rf build-android-arm64 && mkdir build-android-arm64 && cd build-android-arm64 && cmake -G 'MSYS Makefiles' -DCMAKE_TOOLCHAIN_FILE=\"$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake\" -DCMAKE_ANDROID_STL_TYPE=c++_static -DBUILD_ANDROID=1 -DANDROID_ABI=arm64-v8a -DANDROID_STL=c++_static -DANDROID_TOOLCHAIN=clang -DCMAKE_BUILD_TYPE=Release -DSTRIP_ANDROID_LIBRARY=On .. && make -j$(nproc)"
    if errorlevel 1 (
        echo [ARM64] FAILED!
        set "FAIL=1"
    ) else (
        echo [ARM64] OK
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
