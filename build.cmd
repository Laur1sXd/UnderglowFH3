@echo off
setlocal
if "%VSCMD_ARG_TGT_ARCH%"=="x64" goto ready
for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
if not defined VSINSTALL exit /b 1
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
:ready
pushd "%~dp0src"
if not exist build mkdir build
cl /nologo /LD /std:c++20 /O2 /W4 /WX /wd4127 /EHsc /MT /Fo"build\\" Underglow.cpp pe.cpp /link /OUT:"build\Underglow.dll" /IMPLIB:"build\Underglow.lib" /DYNAMICBASE /NXCOMPAT
set "BUILD_RESULT=%ERRORLEVEL%"
popd
exit /b %BUILD_RESULT%
