#ifndef PackageDirectory
  #error PackageDirectory is required
#endif
#ifndef AppVersion
  #error AppVersion is required
#endif

[Setup]
AppId={{297FDF24-8CA6-4D7C-98BA-B4533A861BDB}
AppName=Pixel Workshop
AppVersion={#AppVersion}
AppVerName=Pixel Workshop {#AppVersion} 候选版
AppPublisher=GreenLoong
AppPublisherURL=https://github.com/GreenLoong/pixel-workshop
AppSupportURL=https://github.com/GreenLoong/pixel-workshop/issues
DefaultDirName={localappdata}\Programs\Pixel Workshop
DefaultGroupName=Pixel Workshop
DisableProgramGroupPage=yes
DisableDirPage=no
PrivilegesRequired=lowest
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0.17763
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
SetupIconFile={#SourcePath}\..\..\ImageBatchTool\resources\icons\app-icon.ico
UninstallDisplayIcon={app}\ImageBatchTool.exe
VersionInfoVersion={#AppVersion}
VersionInfoDescription=Pixel Workshop Windows x64 installer
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#PackageDirectory}\*"; DestDir: "{app}"; Excludes: "verification-result.json"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Pixel Workshop"; Filename: "{app}\ImageBatchTool.exe"; WorkingDir: "{app}"
Name: "{group}\Pixel Workshop 自检"; Filename: "{app}\PixelWorkshopCheck.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Pixel Workshop"; Filename: "{app}\ImageBatchTool.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\ImageBatchTool.exe"; WorkingDir: "{app}"; Description: "{cm:LaunchProgram,Pixel Workshop}"; Flags: nowait postinstall skipifsilent

[CustomMessages]
chinesesimplified.RuntimeFailed=无法安装 Microsoft Visual C++ x64 运行库。请允许系统权限提示，或先安装微软官方运行库后重试。退出代码：%1
english.RuntimeFailed=Microsoft Visual C++ x64 runtime installation failed. Allow the elevation prompt or install the official Microsoft runtime first. Exit code: %1

[Code]
function RuntimeFileReady(const FileName: String): Boolean;
var
  VersionText: String;
  InstalledVersion, MinimumVersion: Int64;
begin
  Result := False;
  { 最低值取本版实际验证的运行环境；安装包另带微软较新的官方运行库。 }
  if GetVersionNumbersString(ExpandConstant('{sys}\') + FileName, VersionText) and
     StrToVersion(VersionText, InstalledVersion) and
     StrToVersion('14.50.35719.0', MinimumVersion) then
    Result := ComparePackedVersion(InstalledVersion, MinimumVersion) >= 0;
end;

function RuntimeReady: Boolean;
begin
  Result := RuntimeFileReady('msvcp140.dll') and
            RuntimeFileReady('vcruntime140.dll') and
            RuntimeFileReady('vcruntime140_1.dll');
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ExitCode: Integer;
  Started: Boolean;
begin
  Result := '';
  if RuntimeReady then Exit;
  ExtractTemporaryFile('vc_redist.x64.exe');
  ExitCode := -1;
  if IsAdmin then
    Started := Exec(ExpandConstant('{tmp}\vc_redist.x64.exe'), '/install /passive /norestart', '', SW_SHOW, ewWaitUntilTerminated, ExitCode)
  else
    Started := ShellExec('runas', ExpandConstant('{tmp}\vc_redist.x64.exe'), '/install /passive /norestart', '', SW_SHOW, ewWaitUntilTerminated, ExitCode);
  if Started and ((ExitCode = 0) or (ExitCode = 3010) or (ExitCode = 1638)) and RuntimeReady then
    NeedsRestart := ExitCode = 3010
  else
    Result := FmtMessage(CustomMessage('RuntimeFailed'), [IntToStr(ExitCode)]);
end;
