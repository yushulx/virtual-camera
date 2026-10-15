Virtual Camera (portable / ZIP)

This is the portable layout. The GUI is in the app\ folder and the
DirectShow capture filter in driver\.

To use it without the installer, register the filter once from an
administrator command prompt:

    regsvr32 driver\VirtualCameraDriver.dll

(or run scripts\register-driver.cmd as administrator)

Start app\VirtualCamera.exe, pick a video or image file and press Start.
The camera then shows up as "Virtual Camera" in Chrome, Edge, Firefox,
Zoom, OBS and every other DirectShow application.

To remove the registration:

    regsvr32 /u driver\VirtualCameraDriver.dll

Uninstall = delete the folder.
