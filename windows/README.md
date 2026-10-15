# Windows virtual camera

A DirectShow **video capture source filter** registered in the *Video Input
Devices* category. Once registered, it shows up as **"Virtual Camera"** in
Chrome, Edge, Firefox, Zoom, OBS Studio and every other application that
enumerates cameras through DirectShow.

```text
VirtualCamera.exe            writes frames -> Local\VirtualCameraFrame (shared memory)
VirtualCameraDriver.dll      reads them from inside each consumer process
```

The filter is a self-contained C++/COM DLL (no Qt, static CRT) that

* advertises exactly one format: YUYV (YUY2) at the resolution and frame
  rate configured in the Virtual Camera application,
* picks up configuration changes on the next stream start (browsers
  re-enumerate devices on every `getUserMedia` call),
* serves a **moving color test pattern** when the controller is not
  running, so the camera never looks dead,
* registers/unregisters itself with `regsvr32`.

## Install

Run `VirtualCameraSetup-<version>.exe` (administrator rights). It installs
the GUI plus the driver and registers it. Or use the portable ZIP and:

```bat
regsvr32 driver\VirtualCameraDriver.dll     :: admin prompt, once
```

## Build from source

Requirements: CMake 3.21+, Visual Studio 2022 (C++ workload), Qt 6.5+ for
MSVC, Inno Setup 6 (optional, for the installer).

```powershell
powershell -ExecutionPolicy Bypass -File windows\scripts\build-windows.ps1 `
    -Version 1.0.0 -QTDir C:\Qt\6.8.2\msvc2022_64
```

Output in `windows\dist`:

* `VirtualCameraSetup-<version>.exe` - Inno Setup installer
* `VirtualCamera-<version>-win64-portable.zip` - portable layout

Builds only (no packaging): the root `CMakeLists.txt` with the
`Visual Studio 17 2022` generator; the driver lands in
`build\windows\windows\driver\Release\VirtualCameraDriver.dll`.

## Test the camera

1. Start **Virtual Camera**, pick a video or image, press *Start*.
2. Open `docs/web-camera-test.html` in Chrome/Edge/Firefox and select
   **Virtual Camera**.
3. Without the controller running the camera shows the moving test
   pattern.

## Notes

* **Administrator rights** are needed once for the `regsvr32`
  registration (the installer does it automatically).
* Applications that use **Media Foundation** device enumeration (e.g. the
  built-in Windows Camera app, some UWP apps) do not list DirectShow
  cameras. All major browsers do. A Media Foundation virtual camera
  (Windows 11 22H2+) is on the roadmap.
* Changing the resolution/frame rate in the GUI: restart the camera
  stream in the consuming application (a fresh `getUserMedia` is enough
  in browsers).
* ARM64 Windows: the code is architecture independent; build with
  `-Platform ARM64` (the app then runs natively, and native ARM64
  browsers see the filter). The default x64 build covers x64 systems and
  x64 browsers on ARM64 emulation.
