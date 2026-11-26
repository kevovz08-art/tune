[Setup]
AppName=RelapseTune
AppVersion=1.0.0
AppPublisher=RelapseAudio
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=installer
OutputBaseFilename=RelapseTune-Setup
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma

[Files]
Source: "dist\RelapseTune.vst3\*"; DestDir: "{commoncf64}\VST3\RelapseTune.vst3"; Flags: recursesubdirs ignoreversion
