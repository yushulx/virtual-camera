; VirtualCamera.iss - Inno Setup 6 script for the Windows virtual camera.
;
; It installs the controller GUI (VirtualCamera.exe + Qt runtime) and the
; DirectShow capture filter (VirtualCameraDriver.dll), and registers the
; filter so it appears as "Virtual Camera" in Chrome, Edge, Firefox, Zoom,
; OBS and every other DirectShow application.
;
; Build it with the Inno Setup command line compiler (iscc.exe), ideally
; through scripts\build-windows.ps1 which builds and stages everything
; first:
;
;   iscc.exe /DVersion=1.0.0 /DStageDir=<abs path to stage> VirtualCamera.iss
;
; The staging directory (<StageDir>) is produced by
; scripts\build-windows.ps1 and contains:
;
;   app\      VirtualCamera.exe + Qt runtime (windeployqt output)
;   driver\   VirtualCameraDriver.dll
;
; Requires Inno Setup 6 (https://jrsoftware.org/isinfo.php).

#ifndef Version
  #define Version "1.0.0"
#endif

#ifndef StageDir
  #define StageDir "..\dist\stage"
#endif

#define AppName        "Virtual Camera"
#define AppPublisher   "Dynamsoft"
#define AppUrl         "https://github.com/dynamsoft/virtual-camera"

[Setup]
AppId={{E2B7A4C1-8F3D-4B5E-A6C9-0D1E2F3A4B58}
AppName={#AppName}
AppVersion={#Version}
AppVerName={#AppName} {#Version}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}
DefaultDirName={autopf}\VirtualCamera
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=VirtualCameraSetup-{#Version}
Compression=lzma2/max
SolidCompression=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
WizardStyle=modern
UninstallDisplayName={#AppName} {#Version}
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#Version}

[Files]
Source: "{#StageDir}\app\*";  DestDir: "{app}";           Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\driver\*"; DestDir: "{app}\driver";  Flags: ignoreversion

[Run]
; Register the DirectShow filter (needed once, machine wide).
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\driver\VirtualCameraDriver.dll"""; \
  Flags: runhidden; StatusMsg: "Registering the DirectShow camera filter..."
; Offer to launch the controller after installation.
Filename: "{app}\VirtualCamera.exe"; Description: "Launch Virtual Camera"; \
  Flags: nowait postinstall skipifsilent unchecked

[UninstallRun]
Filename: "{sys}\regsvr32.exe"; Parameters: "/u /s ""{app}\driver\VirtualCameraDriver.dll"""; \
  Flags: runhidden; RunOnceId: "UnregisterCameraDriver"

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
function FilterRegistered: Boolean;
begin
  Result := RegKeyExists(HKEY_CLASSES_ROOT,
    'CLSID\{D4A1F5B2-6E38-4C7A-9B2E-1F8D3A5C7E64}\InprocServer32');
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID <> wpFinished then
    Exit;

  if not FilterRegistered() then
    MsgBox('The DirectShow filter does not seem to be registered.' + #13#10#13#10 +
           'Run this command as administrator and restart your browser:' + #13#10 +
           '    regsvr32 "' + ExpandConstant('{app}') + '\driver\VirtualCameraDriver.dll"',
           mbInformation, MB_OK)
  else
    MsgBox('Virtual Camera is installed.' + #13#10#13#10 +
           'Start "Virtual Camera" from the Start menu, pick a video or image, ' +
           'press Start, then select "Virtual Camera" in your browser or conferencing app.',
           mbInformation, MB_OK);
end;
