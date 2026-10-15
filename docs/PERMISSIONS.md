# Permissions and privileges per platform

## Windows

* **Administrator rights, once**: registering the DirectShow source filter
  (`regsvr32 VirtualCameraDriver.dll`) writes to
  `HKEY_CLASSES_ROOT\CLSID\...`. The Inno Setup installer (and the
  uninstaller) do this automatically; the portable ZIP ships
  `scripts\register-driver.cmd` for the same purpose.
* The camera is visible to **every user** on the machine (the registration
  is machine-wide) - that is how every DirectShow camera behaves.
* No further prompts: DirectShow devices are not gated by a privacy
  prompt. Applications may still show their own camera permission
  settings (e.g. Chrome's site permissions).
* The GUI itself runs with normal user rights; it only writes to
  `%APPDATA%` (configuration) and the shared memory section
  `Local\VirtualCameraFrame` (session-local).

## Linux

* **Root, once**: loading the kernel module
  (`sudo modprobe v4l2loopback ...`). The package postinst and
  `/etc/modules-load.d` handle it; the options live in
  `/etc/modprobe.d/virtual-camera.conf`.
* The `v4l2loopback-dkms` dependency needs the kernel headers of the
  running kernel (`linux-headers-$(uname -r)`) to build the module.
* Using the camera needs no special privileges: `/dev/videoN` is owned by
  group `video` (0660). If your user is not in that group:
  `sudo usermod -aG video $USER` (then log out/in).
* The controller stores its configuration in
  `~/.config/virtual-camera/config.json`.
* App-side prompts: browsers ask before granting camera access
  (`getUserMedia` permission dialog) exactly as they do for a real webcam.

## macOS

* **System extension approval, once**: since macOS 12.3 virtual cameras
  are system extensions, and activating one always requires approval in
  *System Settings > Login Items & Extensions*. The app's
  *Install camera extension...* button walks you through it.
* **Unsigned builds only**: ad-hoc signed extensions additionally need
  developer mode enabled once: `sudo systemextensionsctl developer on`
  (the one-line installer does it for you). Signed + notarized packages
  built with the `SIGNING_IDENTITY` / `INSTALLER_IDENTITY` secrets skip
  this entirely.
* The .pkg install needs an administrator password
  (`sudo installer ...`).
* The camera is per-user: the extension is activated by the user who
  runs the app.
* App-side prompts: applications that open the camera (e.g. Chrome,
  FaceTime) show the normal macOS **camera privacy** prompt the first
  time - same as with a hardware webcam. Approving the extension does
  **not** automatically grant camera access to apps.
* The controller stores its configuration in
  `~/Library/Application Support/Dynamsoft/VirtualCamera/config.json`.
