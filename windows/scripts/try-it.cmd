@echo off
rem try-it.cmd - one-click local trial for the portable layout:
rem   1. registers the DirectShow camera filter (asks for administrator
rem      rights once, because it writes to HKEY_CLASSES_ROOT)
rem   2. launches the Virtual Camera controller GUI
rem
rem Afterwards: pick a video/image (or leave empty for the test pattern),
rem press Start, then select "Virtual Camera" in web-camera-test.html.

setlocal
set "DRIVER=%~dp0driver\VirtualCameraDriver.dll"

if not exist "%DRIVER%" (
    echo Cannot find %DRIVER%
    exit /b 1
)

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Requesting administrator rights to register the camera filter...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

echo Registering the DirectShow camera filter...
regsvr32 "%DRIVER%"

start "" "%~dp0VirtualCamera.exe"
echo.
echo Virtual Camera is starting. Select "Virtual Camera" in your browser
rem (open web-camera-test.html in this folder to verify).
endlocal
