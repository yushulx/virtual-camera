# Virtual Camera for Windows, Linux and macOS

Virtual (software only) cameras for **Windows**, **Linux** and **macOS**.
Applications and browsers see them as real cameras, but instead of capturing
from hardware they play a local **video file** or a **static image** in a
loop - which makes them handy for testing camera workflows, demos, video
conferencing try-outs and CI without a webcam.

All three implementations share the same behaviour:

| Platform | Integration | Implementation | Package |
| --- | --- | --- | --- |
| Windows | DirectShow video capture source filter | C++ / COM (no runtime dependencies) | `VirtualCameraSetup-<version>.exe` (Inno Setup) |
| Linux | [v4l2loopback](https://github.com/v4l2loopback/v4l2loopback) device node | C++ / Qt 6 | `virtual-camera_<version>_amd64.deb` |
| macOS | CMIO Camera Extension (system extension) | Swift | `VirtualCamera-<version>.pkg` |

Once installed, the camera shows up as **"Virtual Camera"** everywhere a
normal webcam would:

* browser JavaScript - `navigator.mediaDevices.getUserMedia({ video: true })`
  and `enumerateDevices()` (Chrome, Edge, Firefox; on macOS also Safari 17.4+)
* video conferencing and streaming apps (Zoom, Teams, OBS, ...)
* any framework built on the platform camera APIs (DirectShow, V4L2, AVFoundation)

A ready-to-use browser test page is bundled: [docs/web-camera-test.html](docs/web-camera-test.html).

## How it works

```text
              +----------------------------------------------+
              |  VirtualCamera (Qt 6 GUI or CLI)             |
              |  pick file . loop playback . scale to the    |
              |  target resolution . convert RGB -> YUYV    |
              |  pace frames at the configured frame rate    |
              +----------------------+-----------------------+
                                     |
        +----------------------------+----------------------------+
        | (shared memory)            | (device write)             | (POSIX shm)
        v                            v                            v
+---------------------+   +---------------------+   +--------------------------+
| Windows             |   | Linux               |   | macOS                     |
| Local\VirtualCamera |   | /dev/videoN         |   | /virtualcamera_frame shm |
| Frame  (seqlock)    |   | v4l2loopback kernel |   | (seqlock)                 |
|     |               |   | module (installed   |   |     |                     |
|     v               |   | as a package dep)   |   |     v                     |
+---------------------+   +---------------------+   +--------------------------+
| VirtualCamera-      |                              | VirtualCameraExtension   |
| Driver.dll          |                              | .appex (CMIO Camera      |
| DirectShow source   |                              | Extension; the app        |
| filter. Runs inside |                              | activates it once via    |
| every consumer      |                              | OSSystemExtensionRequest) |
| process (Chrome...) |                              +--------------------------+
+---------------------+
```

* The GUI/CLI is the **controller**: it decodes the source (video via
  Qt Multimedia, images via QImageReader), scales it to the configured
  resolution and converts it to YUYV (a.k.a. YUY2).
* On **Windows** the frames go into a named shared memory region; the
  DirectShow source filter (registered in the `Video Input Devices`
  category) picks them up from inside whatever application opened the
  camera. When the controller is not running, the filter serves a moving
  color test pattern, so the camera never looks "dead".
* On **Linux** the controller writes the frames straight into the
  `/dev/videoN` node created by the v4l2loopback kernel module
  (installed and configured by the package).
* On **macOS** the frames go into a POSIX shared memory region read by the
  CMIO Camera Extension; when the controller is not running, the extension
  serves a moving color test pattern.

## Download and install

The installers are built and attached to every release by the
[Build installers](.github/workflows/build-installers.yml) workflow - either
push a `v*` tag or start it from *Actions*. See [docs/INSTALL.md](docs/INSTALL.md)
for the detailed step-by-step guides, including the macOS system extension
approval flow. In short:

**Windows** (administrator rights):

```bat
VirtualCameraSetup-1.0.0.exe     :: installs app + DirectShow filter, registers it
```

**Linux** (Debian/Ubuntu):

```bash
sudo apt install ./virtual-camera_1.0.0_amd64.deb   :: pulls v4l2loopback + loads it
v4l2-ctl --list-devices                            :: shows "Virtual Camera"
```

**macOS** (see [docs/INSTALL.md](docs/INSTALL.md) for the unsigned-package
one-line installer):

```bash
sudo installer -pkg VirtualCamera-1.0.0.pkg -target /
# launch VirtualCamera.app once and approve the system extension prompt
```

Then verify in a browser with [docs/web-camera-test.html](docs/web-camera-test.html)
and select **Virtual Camera**.

## Common behaviour

* A **video file** is played in an infinite loop and re-scaled in real time
  to the configured resolution.
* A **static image** is scaled once and repeated at the configured frame rate.
* Resolution presets 640x480 / 854x480 / 1280x720 / 1920x1080 and frame rates
  10 - 60 fps; the pixel format is YUYV (YUY2), 4:2:2, 16 bits per pixel.
* The configuration (source file, resolution, frame rate, loop) persists
  across restarts (`config.json` in the user's configuration location).
* Changing the resolution or frame rate while applications are using the
  camera: restart the camera/stream inside the application afterwards
  (the same applies to most hardware webcams).

## Build from source

* [windows/README.md](windows/README.md) - one command builds the app, the
  DirectShow filter and the packages:

  ```bat
  windows\scripts\build-windows.ps1 -Version 1.0.0
  ```

* [linux/README.md](linux/README.md):

  ```bash
  ./linux/scripts/make-deb.sh 1.0.0     # build + .deb (deps: Qt 6 dev packages)
  ctest --test-dir build               # unit tests
  ```

* [macos/README.md](macos/README.md):

  ```bash
  ./macos/scripts/build-macos.sh        # app + embedded camera extension
  ./macos/scripts/package.sh 1.0.0      # .pkg (signing optional, see script)
  ```

## Testing on a machine without the target OS

* **Linux**: the frame pipeline is user space; the unit tests
  (`ctest`) cover the YUYV conversion, the loop protocol and the sources.
  When a v4l2loopback device is available (GitHub runners can
  `modprobe` it), the CLI smoke test streams real frames to it.
* **Windows**: the DirectShow filter is a plain in-proc COM DLL that can be
  registered and inspected on any Windows 10/11 x64 machine; no signing is
  needed. `virtual-camera-cli --pattern` produces frames without a GUI.
* **macOS**: the CMIO extension needs a real Mac (or the GitHub `macos`
  runner, as the release workflow does). Without the controller running,
  the extension serves a built-in test pattern, so the camera can be
  verified with nothing but the installed package.

## Roadmap

* Windows on ARM64 filter build (the code is architecture independent; only
  the build matrix needs extending)
* Media Foundation virtual camera (Windows 11 22H2+) alongside DirectShow
  for apps that do not use DirectShow (e.g. the built-in Windows Camera app)
* Universal (arm64 + x86_64) macOS app - the extension is already built
  universal; the Qt app ships arm64 for now

## License

MIT - see [LICENSE](LICENSE). No third-party code is bundled; the DirectShow
filter is a self-contained implementation against the Windows SDK.
