@echo off
setlocal
if "%VSCMD_ARG_TGT_ARCH%"=="x64" goto ready
for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
if not defined VSINSTALL exit /b 1
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
:ready
pushd "%~dp0"
call :edition Standalone-Native 0 0
if errorlevel 1 exit /b 1
call :edition Standalone-Network 0 1
if errorlevel 1 exit /b 1
call :edition ExtraOptions-Native 1 0
if errorlevel 1 exit /b 1
call :edition ExtraOptions-Network 1 1
if errorlevel 1 exit /b 1
popd
exit /b 0
:edition
if not exist "build\%1" mkdir "build\%1"
cl /nologo /LD /std:c++20 /O2 /W4 /WX /wd4127 /EHsc /MT /DUG_SUPPORT_EXTRA_OPTIONS=%2 /DUG_SUPPORT_HORIZON_RADIO_NETWORK=%3 /Fo"build\%1\\" Underglow.cpp pe.cpp /link /OUT:"build\%1\Underglow.dll" /DYNAMICBASE /NXCOMPAT
exit /b %ERRORLEVEL%
