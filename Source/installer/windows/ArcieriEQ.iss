[Setup]
AppName=Arcieri EQ
AppVersion=1.0.0
AppPublisher=Arcieri
DefaultDirName={commonpf64}\Arcieri EQ
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64
OutputDir=..\..\dist
OutputBaseFilename=ArcieriEQ-Installer-Windows
Compression=lzma
SolidCompression=yes

[Files]
Source: "..\..\build\ArcieriEQ_artefacts\Release\VST3\Arcieri EQ.vst3\*"; DestDir: "{commoncf64}\VST3\Arcieri EQ.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
