# Installation guide

Step-by-step installation for all three platforms. After installing, verify
the camera with the bundled browser test page:
[web-camera-test.html](web-camera-test.html) (open it in Chrome, Edge or
Firefox, allow camera access, pick **Virtual Camera**).

| Platform | Package | Camera name shown to apps |
| --- | --- | --- |
| Windows 10/11 x64 | `VirtualCameraSetup-<version>.exe` | Virtual Camera |
| Debian/Ubuntu (amd64) | `virtual-camera_<version>_amd64.deb` | Virtual Camera |
| macOS 13+ | `VirtualCamera-<version>.pkg` | Virtual Camera |

---

## Windows

1. Run `VirtualCameraSetup-<version>.exe`. It asks for **administrator
   rights** - they are needed once to register the DirectShow filter with
   `regsvr32`.
2. Launch **Virtual Camera** from the Start menu.
3. Pick a video or image file (or leave it empty for the test pattern),
   choose the resolution and frame rate, press **Start**.
4. Open [web-camera-test.html](web-camera-test.html) in Chrome, Edge or
   Firefox and allow camera access. The camera appears as
   **Virtual Camera**.

Portable ZIP: unpack anywhere and run
`scripts\register-driver.cmd` once from an administrator prompt.

Uninstall: *Settings > Apps* (or the uninstaller in the app folder); it
unregisters the filter automatically.

## Linux (Debian/Ubuntu)

1. Install the package (it pulls the v4l2loopback kernel module and the
   Qt/GStreamer runtime):

   ```bash
   sudo apt install ./virtual-camera_1.0.0_amd64.deb
   ```

2. Check the device node (created by the postinst `modprobe`):

   ```bash
   v4l2-ctl --list-devices     # shows "Virtual Camera" on /dev/videoN
   ```

3. Launch **Virtual Camera**, pick a source, press **Start**, then select
   **Virtual Camera** in your browser (see the test page above).

Manual module setup (without the package):

```bash
sudo apt install v4l2loopback-dkms linux-headers-$(uname -r)
sudo modprobe v4l2loopback exclusive_caps=1 card_label="Virtual Camera" max_devices=1
```

Headless / CI (no GUI): `virtual-camera-cli --pattern --duration 10`.

## macOS

1. Install the package:

   ```bash
   sudo installer -pkg VirtualCamera-1.0.0.pkg -target /
   ```

   or, for the unsigned CI package, the one-line installer (no Gatekeeper
   dialog, enables system extension developer mode for you):

   ```bash
   curl -fsSL https://raw.githubusercontent.com/dynamsoft/virtual-camera/main/macos/scripts/install.sh | bash
   ```

2. Open **VirtualCamera.app** (Applications folder) and click
   **Install camera extension...** once.
3. macOS opens *System Settings > Login Items & Extensions* - approve the
   **Virtual Camera** camera extension there (this approval is a system
   requirement for any camera extension, signed or not).
4. Back in the app: pick a source, press **Start**, then select **Virtual
   Camera** in the browser (Safari 17.4+ or any Chromium/Firefox).

## Verifying with getUserMedia

The camera behaves like a normal webcam:

```js
const stream = await navigator.mediaDevices.getUserMedia({ video: true });
const [track] = stream.getVideoTracks();
console.log(track.label);   // "Virtual Camera"
```

The `docs/web-camera-test.html` page does this with a live preview, the
device list and useful diagnostics.
