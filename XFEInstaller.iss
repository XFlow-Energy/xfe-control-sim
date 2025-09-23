; ———————————————————————————————————————————————————————————————————
; XFEInstaller.iss
;———————————————————————————————————————————————————————————————————

[Setup]
AppName=              XFE Control-Sim Dependencies
AppVersion=           1.0
AppPublisher=         XFlow Energy
AppPublisherURL=      https://www.xflowenergy.com
DefaultDirName=       {commonpf}\XFEControlSimDeps
DisableProgramGroupPage=yes
DisableDirPage=       no
PrivilegesRequired=    admin
SolidCompression=     yes
Compression=          lzma
ArchitecturesInstallIn64BitMode=x64os

OutputDir=            Output
OutputBaseFilename=   XFEControlSimInstaller

[Tasks]
Name: "install_git";           Description: "Git for Windows";      GroupDescription: "Install Components"
Name: "install_cmake";         Description: "CMake";               GroupDescription: "Install Components"
Name: "install_llvm_mingw";    Description: "llvm-mingw";         GroupDescription: "Install Components"
Name: "install_ninja";         Description: "Ninja";              GroupDescription: "Install Components"
Name: "install_gsl";           Description: "GSL";                GroupDescription: "Install Components"
Name: "install_jansson_ninja"; Description: "Jansson & Ninja";    GroupDescription: "Install Components"
Name: "install_libmodbus";     Description: "libmodbus";          GroupDescription: "Install Components"
Name: "install_python";        Description: "Python";             GroupDescription: "Install Components"
Name: "create_plot_executable";Description: "Create plot executable"; GroupDescription: "Install Components"
Name: "clone_repo";            Description: "Clone GitHub Repo";  GroupDescription: "Install Components"

[Files]
Source: "misc\install-git.ps1";                   DestDir: "{app}\misc"; Flags: ignoreversion
Source: "misc\install-cmake-msi.ps1";             DestDir: "{app}\misc"; Flags: ignoreversion
Source: "misc\install-llvm-mingw.ps1";            DestDir: "{app}\misc"; Flags: ignoreversion
Source: "misc\install-ninja-binary.ps1";          DestDir: "{app}\misc"; Flags: ignoreversion
Source: "misc\install-gsl.ps1";                   DestDir: "{app}\misc"; Flags: ignoreversion
Source: "misc\install-jansson-ninja.ps1";         DestDir: "{app}\misc"; Flags: ignoreversion
Source: "misc\install-libmodbus.ps1";             DestDir: "{app}\misc"; Flags: ignoreversion
Source: "misc\install-python.ps1";                DestDir: "{app}\misc"; Flags: ignoreversion
Source: "misc\create_plot_viewer_executable.ps1"; DestDir: "{app}\misc"; Flags: ignoreversion
Source: "misc\plot_viewer.py";                    DestDir: "{app}\misc"; Flags: ignoreversion
Source: "misc\clone-repo.ps1";                    DestDir: "{app}\misc"; Flags: ignoreversion

[Run]
; All of these use the system PowerShell, run hidden, and wait until done.

Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; \
  Parameters: "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ""{app}\misc\install-git.ps1"" -installDir ""{app}\Git"""; \
  Tasks: install_git; \
  WorkingDir: "{app}\misc"; \
  StatusMsg: "Installing Git for Windows..."; \
  Flags: waituntilterminated runhidden

Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; \
  Parameters: "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ""{app}\misc\install-cmake-msi.ps1"" -installDir ""{app}\CMake"""; \
  Tasks: install_cmake; \
  WorkingDir: "{app}\misc"; \
  StatusMsg: "Installing CMake..."; \
  Flags: waituntilterminated runhidden

Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; \
  Parameters: "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ""{app}\misc\install-llvm-mingw.ps1"" -installDir ""{app}\llvm-mingw"""; \
  Tasks: install_llvm_mingw; \
  WorkingDir: "{app}\misc"; \
  StatusMsg: "Installing llvm-mingw..."; \
  Flags: waituntilterminated runhidden

Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; \
  Parameters: "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ""{app}\misc\install-ninja-binary.ps1"" -installDir ""{app}\Ninja"""; \
  Tasks: install_ninja; \
  WorkingDir: "{app}\misc"; \
  StatusMsg: "Installing Ninja..."; \
  Flags: waituntilterminated runhidden

Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; \
  Parameters: "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ""{app}\misc\install-gsl.ps1"" -installDir ""{app}\gsl"" -clangBin ""{app}\llvm-mingw\bin"" -cmakePath ""{app}\CMake\bin\cmake.exe"" -ninjaPath ""{app}\Ninja\ninja.exe"""; \
  Tasks: install_gsl; \
  WorkingDir: "{app}\misc"; \
  StatusMsg: "Installing GSL..."; \
  Flags: waituntilterminated runhidden

Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; \
  Parameters: "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ""{app}\misc\install-jansson-ninja.ps1"" -installDir ""{app}\jansson"" -clangBin ""{app}\llvm-mingw\bin"" -cmakePath ""{app}\CMake\bin\cmake.exe"" -ninjaPath ""{app}\Ninja\ninja.exe"""; \
  Tasks: install_jansson_ninja; \
  WorkingDir: "{app}\misc"; \
  StatusMsg: "Installing Jansson & Ninja..."; \
  Flags: waituntilterminated runhidden

Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; \
  Parameters: "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ""{app}\misc\install-libmodbus.ps1"" -installDir ""{app}\libmodbus"" -llvmBin ""{app}\llvm-mingw\bin"" -cmakePath ""{app}\CMake\bin\cmake.exe"" -ninjaPath ""{app}\Ninja\ninja.exe"""; \
  Tasks: install_libmodbus; \
  WorkingDir: "{app}\misc"; \
  StatusMsg: "Installing libmodbus..."; \
  Flags: waituntilterminated runhidden

; -- install-python now uses the new PythonDirPage instead of {app}\Python --
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; \
  Parameters: "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ""{app}\misc\install-python.ps1"" -installDir ""{code:GetPythonDir}"""; \
  Tasks: install_python; \
  WorkingDir: "{app}\misc"; \
  StatusMsg: "Installing Python..."; \
  Flags: waituntilterminated runhidden

Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; \
  Parameters: "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ""{app}\misc\create_plot_viewer_executable.ps1"" -scriptPath ""{app}\misc\plot_viewer.py"" -distDir ""{app}"" -pythonInstallDir ""{code:GetPythonDir}"""; \
  Tasks: create_plot_executable; \
  WorkingDir: "{app}\misc"; \
  StatusMsg: "Creating Plot Viewer executable..."; \
  Flags: waituntilterminated runhidden

Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; \
  Parameters: "-NoExit -STA -NoProfile -ExecutionPolicy Bypass -File ""{app}\misc\clone-repo.ps1"" -Destination ""{code:GetCloneDir}"" -Select"; \
  Tasks: clone_repo; \
  WorkingDir: "{app}\misc"; \
  StatusMsg: "Cloning GitHub repo…"; \
  Flags: waituntilterminated

;———————————————————————————————————————————————————————————————————

[UninstallRun]
; ── UNINSTALL CMake ──
Filename: "powershell.exe"; \
  Parameters: "-NonInteractive -NoProfile -ExecutionPolicy Bypass -File ""{app}\misc\install-cmake-msi.ps1"" -Uninstall -installDir ""{app}\CMake"""; \
  RunOnceId: "UninstCMake"; \
  StatusMsg: "⏳ Uninstalling CMake (this may take a moment)…"; \
  Flags: runhidden

; ── UNINSTALL llvm-mingw ──
Filename: "powershell.exe"; \
  Parameters: "-NonInteractive -NoProfile -ExecutionPolicy Bypass -File ""{app}\misc\install-llvm-mingw.ps1"" -Uninstall -installDir ""{app}\llvm-mingw"""; \
  RunOnceId: "UninstLLVM"; \
  StatusMsg: "⏳ Uninstalling llvm-mingw (removing files)…"; \
  Flags: runhidden

; ── UNINSTALL Ninja ──
Filename: "powershell.exe"; \
  Parameters: "-NonInteractive -NoProfile -ExecutionPolicy Bypass -File ""{app}\misc\install-ninja-binary.ps1"" -Uninstall -installDir ""{app}\Ninja"""; \
  RunOnceId: "UninstNinja"; \
  StatusMsg: "⏳ Uninstalling Ninja (removing executable & PATH)…"; \
  Flags: runhidden

; ── UNINSTALL GSL ──
Filename: "powershell.exe"; \
  Parameters: "-NonInteractive -NoProfile -ExecutionPolicy Bypass -File ""{app}\misc\install-gsl.ps1"" -Uninstall -installDir ""{app}\gsl"""; \
  RunOnceId: "UninstGSL"; \
  StatusMsg: "⏳ Uninstalling GSL (deleting libs & build)…"; \
  Flags: runhidden

; ── UNINSTALL Jansson & Ninja ──
Filename: "powershell.exe"; \
  Parameters: "-NonInteractive -NoProfile -ExecutionPolicy Bypass -File ""{app}\misc\install-jansson-ninja.ps1"" -Uninstall -installDir ""{app}\jansson"""; \
  RunOnceId: "UninstJanssonNinja"; \
  StatusMsg: "⏳ Uninstalling Jansson & Ninja (removing combined)…"; \
  Flags: runhidden

; ── UNINSTALL libmodbus ──
Filename: "powershell.exe"; \
  Parameters: "-NonInteractive -NoProfile -ExecutionPolicy Bypass -File ""{app}\misc\install-libmodbus.ps1"" -Uninstall -installDir ""{app}\libmodbus"""; \
  RunOnceId: "UninstLibmodbus"; \
  StatusMsg: "⏳ Uninstalling libmodbus (removing source & binaries)…"; \
  Flags: runhidden

; ── UNINSTALL Git for Windows ──
Filename: "powershell.exe"; \
  Parameters: "-NonInteractive -NoProfile -ExecutionPolicy Bypass -File ""{app}\misc\install-git.ps1"" -Uninstall -installDir ""{app}\Git"""; \
  RunOnceId: "UninstGit"; \
  StatusMsg: "⏳ Uninstalling Git for Windows..."; \
  Flags: runhidden

[UninstallDelete]
; Delete the entire application folder (and everything inside it)
Type: filesandordirs; Name: "{app}"

[Code]
function InternetGetConnectedState(var lpdwFlags: DWORD; dwReserved: DWORD): BOOL;
  external 'InternetGetConnectedState@wininet.dll stdcall';

function InitializeSetup(): Boolean;
var
	Flags: DWORD;
begin
	if not InternetGetConnectedState(Flags, 0) then
	begin
		MsgBox('No internet connection detected. Please connect to the internet and try again.', mbError, MB_OK);
		Result := False;
		exit;
	end;
	Result := True;
end;

var
	PythonDirPage: TInputDirWizardPage;
	CloneDirPage: TInputDirWizardPage;

// forward-declare the handlers
procedure PythonDirPage_OnActivate(Page: TWizardPage); forward;
procedure CloneDirPage_OnActivate(Page: TWizardPage); forward;

procedure InitializeWizard();
begin
	// Clone‐repo page
	CloneDirPage := CreateInputDirPage(
		wpSelectDir,
		'Repository Location',
		'Where should the GitHub repository be cloned?',
		'Select a folder for the repository:',
		False,
		''
	);
	CloneDirPage.OnActivate := @CloneDirPage_OnActivate;
	CloneDirPage.Add('');

	// Python install‐dir page
	PythonDirPage := CreateInputDirPage(
		wpSelectDir,
		'Python Installation Directory',
		'Where should Python be installed?',
		'Select a folder for Python installation, cannot be in same location as XFEControlSimInstaller:',
		False,
		''
	);
	PythonDirPage.OnActivate := @PythonDirPage_OnActivate;
	PythonDirPage.Add('');
end;

procedure CloneDirPage_OnActivate(Page: TWizardPage);
begin
	if CloneDirPage.Values[0] = '' then
		CloneDirPage.Values[0] := ExpandConstant('{userdocs}\xflow_control_sim_repo');
end;

procedure PythonDirPage_OnActivate(Page: TWizardPage);
begin
	if PythonDirPage.Values[0] = '' then
		PythonDirPage.Values[0] := ExpandConstant('{commonpf}\Python');
end;

function GetCloneDir(Value: string): string;
begin
	Result := CloneDirPage.Values[0];
end;

//----------------------------------------------------------------------
//  Before Inno deletes every file under {app}, ask for confirmation.
//  If the user clicks “No,” return False and abort the Uninstall entirely.
//----------------------------------------------------------------------
function InitializeUninstall(): Boolean;
begin
  // Show a “Yes/No” box inside the Uninstall wizard:
  if MsgBox(
     'All installed dependencies (folder'#13#10 +
     '"' + ExpandConstant('{app}') + '"'#13#10#13#10 +
     'will be permanently deleted.'#13#10#13#10 +
     'Do you want to continue?',
     mbConfirmation, MB_YESNO
  ) = IDYES then
    Result := True
  else
    Result := False;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  // no extra actions needed here
end;

function GetPythonDir(Value: string): string;
begin
	Result := PythonDirPage.Values[0];
end;