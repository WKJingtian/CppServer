@echo off
setlocal

set "exe=%.\CppServer.exe"
set "logdir=%~dp0log"

if not exist "%logdir%" mkdir "%logdir%"

for /f "delims=" %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "ts=%%i"
set "log=%logdir%\%ts%.log"

if not exist "%exe%" (
    echo Missing executable: %exe%
    exit /b 1
)

echo Logging to %log%
"%exe%" > "%log%" 2>&1
exit /b %errorlevel%
