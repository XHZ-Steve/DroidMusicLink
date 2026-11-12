#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
[Setup]
AppId={{BC3BDF7F-C71B-4979-9F67-C6E1425874EA}
AppName=DroidMediaLink
AppVersion={#AppVersion}
DefaultDirName={localappdata}\Programs\DroidMediaLink
DefaultGroupName=DroidMediaLink
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\artifacts
OutputBaseFilename=DroidMediaLink-{#AppVersion}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
UninstallDisplayIcon={app}\DroidMediaLink.exe
[Files]
Source: "{#StageDirectory}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\DroidMediaLink"; Filename: "{app}\DroidMediaLink.exe"
[Run]
Filename: "{app}\DroidMediaLink.exe"; Description: "DroidMediaLink"; Flags: nowait postinstall skipifsilent
