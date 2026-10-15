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
  design (v4l2loopback) or falls back to the **test pattern**
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
