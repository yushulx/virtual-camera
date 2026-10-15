# Linux virtual camera

A **v4l2loopback** device node fed by the `virtual-camera` controller.
The package installs the controller, loads the kernel module (labelled
**"Virtual Camera"**, `exclusive_caps=1` so Chrome and Zoom accept it)
and wires everything up at boot.

## Install

```bash
sudo apt install ./dist/virtual-camera_1.0.0_amd64.deb
v4l2-ctl --list-devices        # shows the "Virtual Camera" node
```

The package pulls in `v4l2loopback-dkms` (builds the module for your
kernel, needs `linux-headers-$(uname -r)`), configures
`/etc/modprobe.d/virtual-camera.conf`:

```
options v4l2loopback exclusive_caps=1 card_label="Virtual Camera" max_devices=1
```

and loads the module via `/etc/modules-load.d/virtual-camera.conf`.

Manual module setup (no package):

```bash
sudo apt install v4l2loopback-dkms
sudo modprobe v4l2loopback exclusive_caps=1 card_label="Virtual Camera" max_devices=1
```

Then start **Virtual Camera**, pick a video or image, press *Start* and
select the camera in the browser (see `docs/web-camera-test.html`).

## Test the camera

1. Start **Virtual Camera** (application menu or `virtual-camera`).
2. Choose the source - the same options as on Windows:
   * **Browse...** for a video file (`mp4`, `mkv`, ...) or an image - or
     **select several images at once** (they are cycled like a slideshow,
     about 2 s each);
   * **Open Folder...** to load a whole folder of images (natural order,
     e.g. `img2.png` before `img10.png`);
   * leave the field empty for the built-in moving test pattern.
3. Pick the resolution and frame rate and press **Start** (the controller
   starts writing frames into the v4l2loopback device).
4. Open [../docs/web-camera-test.html](../docs/web-camera-test.html) in
   Chrome or Firefox, allow camera access and select **Virtual Camera** -
   the picker in the page preselects it automatically.

Headless / CI (no GUI, same pipeline):

```bash
QT_QPA_PLATFORM=offscreen virtual-camera-cli --pattern --duration 10
QT_QPA_PLATFORM=offscreen virtual-camera-cli --source /path/video.mp4 --duration 30
virtual-camera-cli --list-devices
```

## Build from source

Requirements: cmake 3.21+, g++ (C++17), Qt 6 development packages:

```bash
sudo apt install build-essential cmake \
    qt6-base-dev qt6-multimedia-dev \
    libv4l-dev v4l2loopback-dkms v4l2-utils
```

Build, test and package:

```bash
./linux/scripts/make-deb.sh 1.0.0     # or:
cmake -B build && cmake --build build
ctest --test-dir build                # unit tests (no camera needed)
```

## Permissions / troubleshooting

* `sudo modprobe v4l2loopback ...` needs **root**; using the device node
  as a normal user needs no extra permissions (`/dev/videoN` is 0660,
  group `video`; add yourself if needed:
  `sudo usermod -aG video $USER`).
* **Camera not visible in Chrome?** The module must be loaded with
  `exclusive_caps=1` - without it Chrome/Zoom ignore nodes that offer
  both capture and output. Check with
  `v4l2-ctl -d /dev/video0 --all`.
* **Green/empty picture?** Start the controller **before** opening the
  camera in the app; until the first frame is written the buffer is
  empty. The camera keeps showing the last frame when the controller
  stops.
* The dkms build needs the kernel headers of the running kernel
  (`sudo apt install linux-headers-$(uname -r)`).
