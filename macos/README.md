# macOS virtual camera

A **CMIO Camera Extension** (system extension) that provides the
**"Virtual Camera"** device to macOS 13 (Ventura) and newer. The
controller app (VirtualCamera.app) writes YUYV frames into a POSIX
shared memory region; the extension serves them to Chrome, Safari 17.4+,
Firefox, Zoom, FaceTime and every other CMIO/AVFoundation application.

When the controller is not running, the extension serves a **moving
color test pattern** so the camera never looks dead.

```text
VirtualCamera.app                  writes frames -> /virtualcamera_frame (POSIX shm)
VirtualCameraExtension.appex       reads them from inside sysexd (CMIO)
```

## Install

```bash
sudo installer -pkg VirtualCamera-1.0.0.pkg -target /
```

or the one-line installer (no Gatekeeper friction, see below):

```bash
curl -fsSL https://raw.githubusercontent.com/yushulx/virtual-camera/main/macos/scripts/install.sh | bash
```

Then, **once**:

1. Open **VirtualCamera.app** from the Applications folder.
2. Click **Install camera extension...** - macOS shows a dialog asking
   to open *System Settings > Login Items & Extensions*.
3. Approve the **Virtual Camera** extension there (the app finishes the
   activation automatically).
4. Pick a video/image file, press **Start**, then select **Virtual
   Camera** in your browser or conferencing app.

### Why the extra approval / developer mode?

Since macOS 12.3 the only way to provide a virtual camera is a system
extension. Activating one always requires a user approval in System
Settings. Packages built without a Developer ID (the CI default) are
ad-hoc signed, which additionally requires *developer mode* to be
enabled once:

```bash
sudo systemextensionsctl developer on
```

The one-line installer does this for you. For distribution to third
parties, set the `SIGNING_IDENTITY`, `INSTALLER_IDENTITY` and
notarization secrets when building - then no developer mode is needed.

## Build from source

Requirements: CMake 3.21+, Qt 6.5+ (macdeployqt in PATH or `QTDIR` set),
Xcode 15+ (Swift toolchain for the extension).

```bash
./macos/scripts/package.sh 1.0.0 --build   # app + extension + .pkg
./macos/scripts/build-macos.sh              # app only (build/macos/app)
```

The extension is built universal (arm64 + x86_64) from plain `swiftc`
(no Xcode project needed); the Qt app builds for the host architecture
(pass `--universal` + a universal Qt for a fat app).

## Troubleshooting

* **The camera is not listed**: check that the extension is active in
  *System Settings > Login Items & Extensions > Camera Extension*, and
  that the app was launched from the Applications folder (not from a
  translocated/quarantined download).
* **"Install camera extension..." fails**: unsigned builds need
  `sudo systemextensionsctl developer on` first. Signed builds need
  matching team IDs on the app and the appex (the build scripts sign
  both with the same identity).
* **Safari**: virtual cameras require Safari 17.4+; Chrome and Firefox
  work on macOS 13+.
* **Resolution changes**: restart the camera stream in the consuming
  app after changing the resolution in the GUI (a fresh `getUserMedia`
  is enough in browsers).
