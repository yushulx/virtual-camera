@echo off
rem unregister-driver.cmd - remove the DirectShow camera filter registration.

setlocal
set "DRIVER=%~dp0..\driver\VirtualCameraDriver.dll"

if not exist "%DRIVER%" (
    echo Cannot find %DRIVER%
    exit /b 1
)

echo Unregistering %DRIVER% ...
regsvr32 /u "%DRIVER%"
endlocal
