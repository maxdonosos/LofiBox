; =============================================================================
;  Lo-Fi Box — instalador Windows (Inno Setup 6)
;  Compilar:  iscc /DArtefacts=<ruta\LoFiBox_artefacts\Release> /DVersion=1.0.0 LoFiBox.iss
; =============================================================================
#ifndef Version
  #define Version "1.0.0"
#endif
#ifndef Artefacts
  #define Artefacts "..\..\build\LoFiBox_artefacts\Release"
#endif

[Setup]
AppId={{6F2B1C7E-4E3A-4C55-9B0A-1F0B10F1B0C5}
AppName=Lo-Fi Box
AppVersion={#Version}
AppPublisher=LoFiBox Audio
DefaultDirName={autopf}\Lo-Fi Box
DefaultGroupName=Lo-Fi Box
OutputDir=..\..\dist
OutputBaseFilename=LoFiBox-{#Version}-windows-setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
DisableProgramGroupPage=yes
UninstallDisplayName=Lo-Fi Box

[Types]
Name: "full";   Description: "Full (VST3 + standalone)"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3";       Description: "VST3 plug-in  (C:\Program Files\Common Files\VST3)"; Types: full custom
Name: "standalone"; Description: "Standalone app";                                     Types: full

[Files]
Source: "{#Artefacts}\VST3\Lo-Fi Box.vst3\*"; DestDir: "{commoncf64}\VST3\Lo-Fi Box.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Artefacts}\Standalone\Lo-Fi Box.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Lo-Fi Box"; Filename: "{app}\Lo-Fi Box.exe"; Components: standalone
