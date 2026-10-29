# Troubleshooting

## The camera is not listed at all

**Windows**
* Check the registration:
  `reg query "HKCR\CLSID\{D4A1F5B2-6E38-4C7A-9B2E-1F8D3A5C7E64}\InprocServer32"`.
  If missing, run as administrator:
  `regsvr32 "C:\Program Files\VirtualCamera\driver\VirtualCameraDriver.dll"`.
* Applications that use **Media Foundation** enumeration (the built-in
  Windows Camera app, some UWP apps) do not list DirectShow cameras -
  that is expected. All major browsers do.
* Restart the application after registering - device lists are cached at
  startup (browsers re-enumerate per `getUserMedia`).

**Linux**
* `v4l2-ctl --list-devices` should show a device with card label
  **Virtual Camera**. If not:
  `lsmod | grep v4l2loopback`, then
  `sudo modprobe v4l2loopback exclusive_caps=1 card_label="Virtual Camera" max_devices=1`.
* Chrome and Zoom refuse v4l2loopback devices **without
  `exclusive_caps=1`** (they skip nodes advertising both capture and
  output capabilities). The package writes the option to
  `/etc/modprobe.d/virtual-camera.conf`; if the module was loaded before
  without it: `sudo modprobe -r v4l2loopback` and reload with the option.
* dkms build failed? Install the headers of the running kernel:
  `sudo apt install linux-headers-$(uname -r)`, then
  `sudo dkms autoinstall`.

**macOS**
* Check *System Settings > Login Items & Extensions > Camera Extension*:
  **Virtual Camera** must be enabled. If the activation failed, unsigned
  builds need `sudo systemextensionsctl developer on` first, then press
  *Install camera extension...* again.
* `systemextensionsctl list` should show
  `com.dynamsoft.virtualcamera.VirtualCameraExtension` as
  `[activated enabled]`.
* Safari needs 17.4+ for system extension cameras; Chrome/Edge/Firefox
  work on macOS 13+.

## Black, green or frozen picture

* The camera shows the **last frame** when the controller stops - by
  design (v4l2loopback), or for 2 seconds and then the **test pattern**
  (DirectShow/CMIO). A green/garbled picture on Linux means the consumer
  opened the camera before the writer produced a frame: start the
  controller first, then (re)open the camera in the app.
* Video file does not play?
  * Linux: the GStreamer runtime packages must be installed
    (`gstreamer1.0-plugins-good`, `gstreamer1.0-libav`); try
    `gst-play-1.0 yourfile.mp4`.
  * Windows/macOS: the OS decoders handle MP4/H.264; exotic codecs
    (HEVC on Windows without the extension, AV1 without support) will
    not decode.
* Try a static image first to isolate decode problems.

## The first frame is green

Fixed in 1.0.7. On 1.0.6 and older the shared memory region (and the V4L2
node on Linux) started out zeroed, and an all-zero YUYV frame is not black:
Y=0 with U=V=0 decodes to saturated green. A video source needs a few
hundred milliseconds to open its decoder, so that green buffer was on screen
until the first real frame arrived. Both writers now publish a limited-range
black frame (Y=16, U=V=128) before the first frame.

If you still see green on 1.0.7, check that the consumer selected the right
device and that the controller is the new build.

## The camera is missing after OBS (or another virtual camera) was installed

Two virtual cameras do not conflict at the registration level: each one is a
separate COM class in the `Video Input Devices` category, with its own CLSID
and its own device name (`Virtual Camera` here, `OBS Virtual Camera` for
OBS). Registering or unregistering one does not touch the other.

What does make a camera "disappear" in that situation:

* **The application enumerated devices before the camera existed.** Chrome,
  Zoom, Teams and most desktop apps read the camera list when they start.
  Install or register the filter, then fully quit and reopen the application
  - closing the tab is not enough for a browser.
* **The application is 32-bit.** This filter is registered for 64-bit
  consumers only. A 32-bit host sees the 32-bit part of the registry, where
  the filter does not exist, so it lists only cameras that also ship a
  32-bit filter.
* **Camera access is blocked for that app.** *Settings > Privacy & security >
  Camera* has a per-application switch on Windows 10/11.
* **The registration never happened.** Reinstalling repairs it, or register
  the DLL under `C:\Program Files\VirtualCamera\driver` from an administrator
  prompt and reopen the app.

Run the bundled diagnostic to see which of these applies:

```bat
enum-cameras.exe
```

It prints every enumerated device with its CLSID, the DLL that serves it, the
architecture of that DLL and whether a 32-bit registration exists, then a
summary that names this project's filter and OBS's if present.

## Changed resolution/frame rate but the app still shows the old size

Restart the camera stream in the consuming application (browsers: a
fresh `getUserMedia`; Zoom: switch camera away and back). Devices are
enumerated at stream start, like with hardware webcams.

## High CPU / dropped frames

* Prefer 720p@30 for conferencing scenarios; 1080p YUYV means ~8 MB per
  frame through the pipeline.
* Video sources: the GUI decodes with the platform media stack
  (Media Foundation / GStreamer / AVFoundation) - the CPU cost is the
  decode, not the camera.

## Windows-specific

* `regsvr32` fails with "the module was loaded but the entry point was
  not found": use the 64-bit `regsvr32` from `C:\Windows\System32` (the
  default in a 64-bit shell).
* The DLL logs to the debugger output (DebugView) in debug builds only;
  release builds are silent by design.

## Logs and diagnostics

* Browser: the test page (`docs/web-camera-test.html`) prints the
  enumerated devices, the chosen track label and `track.getSettings()`.
* Linux: `v4l2-ctl -d /dev/videoN --all --stream-mmap --stream-count=1`.
* macOS: `systemextensionsctl list`, Console.app (filter: "VirtualCamera").
* CLI smoke test everywhere:
  `virtual-camera-cli --pattern --duration 5 --width 1280 --height 720`.
