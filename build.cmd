@echo off
REM Build the VTA-Simulator using the VS Build Tools compiler + bundled CMake/Ninja + vcpkg libraries.
set "VSBT=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
call "%VSBT%\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=%VSBT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSBT%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
cd /d "E:\Uni Siegen\PEP\VTA-Simulator-main"
echo ===== CONFIGURE =====
cmake --preset vcpkg || exit /b 1
echo ===== BUILD =====
cmake --build build || exit /b 1
echo ===== BUILD OK =====
