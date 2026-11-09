@echo off
setlocal
rem Older MSVC versions warn about intentional compile-time edition checks.
set "CL=%CL% /wd4127"
if "%~1"=="" goto build
if /I "%~1"=="Standalone" goto build
if /I "%~1"=="ExtraOptions" (
    set "CL=%CL% /DUG_SUPPORT_EXTRA_OPTIONS=1"
    goto build
)
echo Usage: build.cmd [Standalone^|ExtraOptions]
exit /b 2

:build
call "%~dp0src\build.cmd"
exit /b %ERRORLEVEL%
