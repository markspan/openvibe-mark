; UG-Lab cumulative patch for OpenViBE 3.7.0
#ifndef PatchVersion
  #define PatchVersion "0.0.0"
#endif

[Setup]
AppName=OpenViBE UG-Lab Patch
AppVersion={#PatchVersion}
AppPublisher=UG-Lab
DefaultDirName={autopf}\openvibe-3.7.0
DisableDirPage=no
DisableProgramGroupPage=yes
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=..\installer
OutputBaseFilename=patch-ug-{#PatchVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=OpenViBE UG-Lab Patch {#PatchVersion}
CreateUninstallRegKey=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\meta\build\bin\openvibe-acquisition-server.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "..\meta\build\bin\openvibe-plugins-simple-visualization.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "..\meta\build\bin\openvibe-plugins-contrib-file-io.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "..\meta\build\share\openvibe\applications\acquisition-server\interface-EEGO-UG.ui"; DestDir: "{app}\share\openvibe\applications\acquisition-server"; Flags: ignoreversion
Source: "..\meta\build\share\openvibe\plugins\simple-visualization\openvibe-simple-visualization-SignalDisplay.ui"; DestDir: "{app}\share\openvibe\plugins\simple-visualization"; Flags: ignoreversion

[Code]
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectDir then
    if not FileExists(ExpandConstant('{app}\bin\openvibe-kernel.dll')) then
    begin
      MsgBox('The selected folder does not appear to contain OpenViBE 3.7.0.' + #13#10 +
             'Please select the correct installation folder.', mbError, MB_OK);
      Result := False;
    end;
end;

function InitializeSetup(): Boolean;
var
  candidates: TArrayOfString;
  i: Integer;
begin
  Result := True;
  SetArrayLength(candidates, 3);
  candidates[0] := 'C:\Program Files\openvibe-3.7.0';
  candidates[1] := 'C:\Program Files (x86)\openvibe-3.7.0';
  candidates[2] := 'C:\openvibe-3.7.0';
  for i := 0 to 2 do
    if FileExists(candidates[i] + '\bin\openvibe-kernel.dll') then
    begin
      WizardForm.DirEdit.Text := candidates[i];
      Break;
    end;
end;