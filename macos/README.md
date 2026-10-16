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

## Test the camera

1. Launch **VirtualCamera.app** (from the Applications folder).
2. Choose the source - the same options as on Windows:
   * **Browse...** for a video file (`mp4`, `mov`, ...) or an image - or
     **select several images at once** (they are cycled like a slideshow,
     about 2 s each);
   * **Open Folder...** to load a whole folder of images (natural order);
   * leave the field empty for the built-in moving test pattern.
3. Pick the resolution and frame rate and press **Start**.
4. Open [../docs/web-camera-test.html](../docs/web-camera-test.html) in
   Chrome, Firefox or Safari (17.4+), allow camera access and select
   **Virtual Camera** - the picker in the page preselects it automatically.
   The camera also appears in FaceTime, Zoom and any AVFoundation app.

No controller running? The camera extension keeps serving the moving test
pattern, which is a quick way to verify the installation.

### Why the extra approval / developer mode?

Since macOS 12.3 the only way to provide a virtual camera is a system
extension. Activating one always requires a user approval in System
Settings. Packages built without a Developer ID (the CI default) are
ad-hoc signed, which additionally requires *developer mode* to be
enabled once:

```bash
sudo systemextensionsctl developer on
```

The one-line installer tries to do this for you. **Note:** on macOS 15+ /
26 the `developer on` command fails while SIP is enabled - in that case
follow [the SIP steps below](#macos-15--26-developer-mode-requires-disabling-sip-first).
For distribution to third
parties, set the `SIGNING_IDENTITY`, `INSTALLER_IDENTITY` and
notarization secrets when building - then no developer mode is needed.

### macOS 15+ / 26+: developer mode requires disabling SIP first

On recent macOS versions, `sudo systemextensionsctl developer on` fails
while System Integrity Protection is enabled:

```text
At this time, this tool cannot be used if System Integrity Protection
is enabled.
```

Until Apple removes this limitation, enabling developer mode is a
one-time, reboot-heavy process:

**Step 1 - Disable SIP (one time):**

1. Shut down the Mac.
2. Press and hold the power button until *Loading startup options*
   appears, then click **Options > Continue** to enter Recovery mode.
3. Open **Utilities > Terminal** in the menu bar and run:

   ```bash
   csrutil disable
   ```

4. Reboot into macOS.

**Step 2 - Enable developer mode and activate the extension:**

```bash
sudo systemextensionsctl developer on
```

Then open **VirtualCamera.app**, click **Install camera extension...**
and approve it in *System Settings > General > Login Items & Extensions
> Camera Extension*.

**Step 3 - Verify:**

```bash
systemextensionsctl list          # look for Camera Extension [activated enabled]
system_profiler SPCameraDataType  # "Virtual Camera" should be listed
```

Once verified, refresh the browser page (or re-open the camera picker)
and **Virtual Camera** appears next to the built-in FaceTime camera.

**Step 4 - Re-enable SIP (recommended):**

Repeat step 1 with `csrutil enable`. Already-activated extensions keep
working; only *new* activations need developer mode again.

> **Tip:** the only way to avoid this entire dance is a properly signed
> and notarized build (`SIGNING_IDENTITY` + `INSTALLER_IDENTITY`), which
> needs the `com.apple.developer.system-extension.install` entitlement
> provisioned by Apple for CMIO camera extensions.

## Build from source

Requirements: CMake 3.21+, Qt 6.4+ (macdeployqt in PATH or `QTDIR` set),
Xcode 15+ (Swift toolchain for the extension).

```bash
./macos/scripts/package.sh 1.0.0 --build   # app + extension + .pkg
./macos/scripts/build-macos.sh              # app only (build/macos/app)
```

The extension is built universal (arm64 + x86_64) from plain `swiftc`
(no Xcode project needed); the Qt app builds for the host architecture
(pass `--universal` + a universal Qt for a fat app).

## Troubleshooting

* **The app won't launch** (`open` reports
  `RBSRequestErrorDomain Code=5` / `NSPOSIXErrorDomain Code=163`):
  three independent causes, all seen in the wild:

  1. **Restricted entitlement on an ad-hoc signature.**
     `com.apple.developer.system-extension.install` requires an
     Apple-provisioned profile. Ad-hoc builds signed *with* this
     entitlement are killed at spawn (no crash report). Fix: sign
     ad-hoc builds **without** entitlements - with developer mode
     enabled the entitlement is not needed. (`build-macos.sh` already
     does this.)
  2. **Broken nested signatures.** `macdeployqt` copies/patches Qt
     frameworks and plugin dylibs after their signatures were created.
     `codesign --verify --deep --strict /Applications/VirtualCamera.app`
     reports *"invalid resource directory"*. Re-sign everything from
     the inside out:

     ```bash
     # bare dylibs: Qt plugins AND helpers at the top of Frameworks/
     sudo find /Applications/VirtualCamera.app/Contents \
          -name '*.dylib' -type f -exec codesign --force --sign - {} +
     # Qt frameworks
     sudo find /Applications/VirtualCamera.app/Contents/Frameworks \
          -maxdepth 1 -name '*.framework' -exec codesign --force --sign - {} +
     # extension, then the app itself
     sudo codesign --force --sign - \
          /Applications/VirtualCamera.app/Contents/PlugIns/VirtualCameraExtension.appex
     sudo codesign --force --sign - /Applications/VirtualCamera.app
     ```

  3. **macdeployqt misses QtDBus** (Homebrew Qt is built with D-Bus
     support, so QtGui links `QtDBus.framework` but it is not
     deployed). The app spawns, then dies immediately with a
     `Library not loaded: @rpath/QtDBus.framework` crash report.
     Fix: copy the missing framework before signing:

     ```bash
     cp -R "$(dirname "$(command -v macdeployqt)")/../lib/QtDBus.framework" \
          /Applications/VirtualCamera.app/Contents/Frameworks/
     # ...then re-sign as in (2)
     ```

     (`build-macos.sh` now scans all Mach-O references and copies any
     missing Qt framework automatically.)

  A passing `codesign --verify --deep --strict` only proves the
  signatures are intact - it does NOT catch (1) or (3), which is why
  each cause above had to be found at launch time.
* **The camera is not listed**: check that the extension is active in
  *System Settings > Login Items & Extensions > Camera Extension*, and
  that the app was launched from the Applications folder (not from a
  translocated/quarantined download). Note that a *running app* does
  not mean an *activated extension*: if
  `systemextensionsctl list` shows no camera extension, no
  **Virtual Camera** device exists for browsers to pick up - see the
  developer mode steps above.
* **"Install camera extension..." fails**: unsigned builds need
  `sudo systemextensionsctl developer on` first. Signed builds need
  matching team IDs on the app and the appex (the build scripts sign
  both with the same identity).
* **Safari**: virtual cameras require Safari 17.4+; Chrome and Firefox
  work on macOS 13+.
* **Resolution changes**: restart the camera stream in the consuming
  app after changing the resolution in the GUI (a fresh `getUserMedia`
  is enough in browsers).
