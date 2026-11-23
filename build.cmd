@echo off
setlocal
rem Preserve the original 1.0.0 sources; select the edition at compile time.
set "CL=%CL% /wd4127"
if "%~1"=="" goto extra
if /I "%~1"=="ExtraOptions" goto extra
if /I "%~1"=="Standalone" (
    set "CL=%CL% /DUG_SUPPORT_EXTRA_OPTIONS=0"
    goto build
)
echo Usage: build.cmd [ExtraOptions^|Standalone]
exit /b 2
:extra
set "CL=%CL% /DUG_SUPPORT_EXTRA_OPTIONS=1"
:build
call "%~dp0src\build.cmd"
exit /b %ERRORLEVEL%
