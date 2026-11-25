@echo off
rem Read docs/CUSTOM_RELEASE_BUILD.md before building. Redirect this script's output to a log.
set "upload_reg_root=%~dp0.."
set "upload_reg_out=%upload_reg_root%\analysis\d3d12-upload-general"
if not exist "%upload_reg_out%" mkdir "%upload_reg_out%"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /EHsc /MD /std:c++17 /O2 /I "C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um" /I "%upload_reg_root%\renderdoc" "%upload_reg_root%\scripts\d3d12_upload_regression.cpp" /Fo:"%upload_reg_out%\d3d12_upload_regression.obj" /Fe:"%upload_reg_out%\d3d12_upload_regression.exe" /link d3d12.lib dxgi.lib
exit /b %errorlevel%
