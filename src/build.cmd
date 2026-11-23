@echo off
setlocal
set "BUILD_DIR=build"
if "%VSCMD_ARG_TGT_ARCH%"=="x64" goto build
for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
if not defined VSINSTALL (
    echo Install Visual Studio Build Tools with Desktop development with C++.
    exit /b 1
)
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
:build
pushd "%~dp0"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"
cl.exe /nologo /LD /std:c++17 /O2 /W4 /WX /EHsc /MT "%~dp0Underglow.cpp" "%~dp0pe.cpp" /link /OUT:Underglow.dll /DYNAMICBASE /NXCOMPAT
set "BUILD_RESULT=%ERRORLEVEL%"
popd
exit /b %BUILD_RESULT%
