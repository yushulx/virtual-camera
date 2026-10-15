@echo off
rem register-driver.cmd - register the portable DirectShow camera filter.
rem Run as administrator (the installer does this automatically).

setlocal
set "DRIVER=%~dp0..\driver\VirtualCameraDriver.dll"

if not exist "%DRIVER%" (
    echo Cannot find %DRIVER%
    echo Build first: powershell -ExecutionPolicy Bypass -File "%~dp0build-windows.ps1"
    exit /b 1
)

echo Registering %DRIVER% ...
regsvr32 "%DRIVER%"
endlocal
