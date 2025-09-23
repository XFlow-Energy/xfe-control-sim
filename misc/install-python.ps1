<#
.SYNOPSIS
  Install Python 3 by downloading the official installer (MSI/EXE) of the latest Python 3.x release,
  run it silently into $installDir, and register an Add/Remove Programs entry.

.PARAMETER installDir
  Directory where you want to put Python’s runtime (e.g. "{app}\Python").

.EXAMPLE
  .\install-python.ps1
#>

param(
	[string] $installDir = "C:\Program Files\XFEControlSimDeps\Python"
)

# Start logging into the same folder the script lives in
$logPath = Join-Path $PSScriptRoot 'install.log'
Start-Transcript -Path $logPath -Append

$ErrorActionPreference = 'Stop'

function Get-PythonInfo {
	<#
	.SYNOPSIS
	  Returns @($exeName, $versionString) if “python.exe” or “py” is on PATH and is 3.x,
	  otherwise returns @($null, $null).
	#>
	try {
		$versionOutput = & python --version 2>&1
		if ($versionOutput -match '^Python\s+3\.\d+\.\d+') { return @("python", $versionOutput.Trim()) }
	} catch { }
	try {
		$versionOutput = & py -3 --version 2>&1
		if ($versionOutput -match '^Python\s+3\.\d+\.\d+') { return @("py", $versionOutput.Trim()) }
	} catch { }
	return @($null, $null)
}

function Get-LatestPythonVersion {
	<#
	.SYNOPSIS
	  Scrape https://www.python.org/downloads/ for the first “Download Python X.Y.Z” link,
	  extract that version string (e.g. 3.13.4).
	#>
	Write-Host "Querying python.org for the Latest Python 3 Release…" -NoNewline
	try {
		$html = Invoke-RestMethod -Uri "https://www.python.org/downloads/" -UseBasicParsing
	} catch {
		throw "ERROR: Failed to download https://www.python.org/downloads/: $($_.Exception.Message)"
	}
	if ($html -notmatch 'Download Python (?<ver>\d+\.\d+\.\d+)') {
		throw "ERROR: Could not locate a 'Download Python <version>' link on https://www.python.org/downloads/."
	}
	return $Matches.ver
}

function Remove-FromMachinePath($folderToRemove) {
	$envName  = "PATH"
	$existing = [Environment]::GetEnvironmentVariable($envName, [EnvironmentVariableTarget]::Machine)
	if (-not $existing) { return }
	$newParts = $existing -split ";" | Where-Object { $_ -and ($_ -ne $folderToRemove) }
	$newValue = $newParts -join ";"
	[Environment]::SetEnvironmentVariable($envName, $newValue, [EnvironmentVariableTarget]::Machine)
}

# ──────────────────────────────────────────────────────────────────────────
$PythonExe = Join-Path $installDir "python.exe"
if (Test-Path $PythonExe) {
	Write-Host "Regular Python detected at $PythonExe."
	$versionFound = (& $PythonExe --version 2>&1) -replace '^Python\s+',''
	$PythonVer    = "Regular Python"
	$env:PATH     = "$installDir;$installDir\Scripts;$env:PATH"
} else {
	$pyInfo    = Get-PythonInfo
	$PythonExe = $pyInfo[0]
	$PythonVer = $pyInfo[1]
}

if ($PythonExe) {
	Write-Host "Python detected: $PythonExe ($PythonVer). Skipping installer download."
} else {
	Write-Host "Python 3 not on PATH - installing via official MSI..."

	$isAdmin = (New-Object Security.Principal.WindowsPrincipal(
				[Security.Principal.WindowsIdentity]::GetCurrent()
			)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
	if (-not $isAdmin) {
		Write-Host "ERROR: This script must be run as Administrator." -ForegroundColor Red
		exit 1
	}

	try {
		$versionFound = Get-LatestPythonVersion
		Write-Host " -> Found version $versionFound"
	} catch {
		Write-Host $_
		exit 1
	}

	# Download installer
	$installerName = "python-$versionFound-amd64.exe"
	$installerUrl  = "https://www.python.org/ftp/python/$versionFound/$installerName"
	$tmpInstaller  = Join-Path $env:TEMP $installerName

	Write-Host "Downloading installer $installerUrl ..."
	try {
		Invoke-WebRequest -Uri $installerUrl -OutFile $tmpInstaller -UseBasicParsing -ErrorAction Stop
		Write-Host "Download complete: $tmpInstaller"
	} catch {
		Write-Host ("ERROR: Failed to download {0}: {1}" -f $installerUrl, $_.Exception.Message)
		exit 1
	}

	# Create installDir if missing
	if (-not (Test-Path $installDir)) {
		Write-Host "Creating install directory: $installDir ..."
		New-Item -ItemType Directory -Path $installDir | Out-Null
	}

	Write-Host "Running Python installer silently into $installDir …"
	$logPath = Join-Path $env:TEMP "python_install_$versionFound.log"

	# Disable ARP entry & shortcuts: Include_launcher=0, Shortcuts=0, ARPSYSTEMCOMPONENT=1
	$installArgs = "/quiet InstallAllUsers=1 PrependPath=0 TargetDir=`"$installDir`" Include_pip=1 Include_dev=0 Include_experimental=0 Include_launcher=0 Shortcuts=0 ARPSYSTEMCOMPONENT=1 NoRestart=1 Log=`"$logPath`""

	$proc = Start-Process -FilePath $tmpInstaller -ArgumentList $installArgs -Wait -PassThru
	if ($proc.ExitCode -ne 0) {
		Write-Host "ERROR: Python installer exited with code $($proc.ExitCode)." -ForegroundColor Red
		Write-Host "  (See detailed log: $logPath)" -ForegroundColor Yellow
		exit 1
	}

	# Clean up installer
	Remove-Item -Path $tmpInstaller -Force -ErrorAction SilentlyContinue

	# Verify python.exe
	$pythonExePath = Join-Path $installDir "python.exe"
	if (-not (Test-Path $pythonExePath)) {
		Write-Host "ERROR: python.exe not found under $installDir after installation." -ForegroundColor Red
		Write-Host "  (Check log at $logPath for details.)" -ForegroundColor Yellow
		exit 1
	}

	$PythonExe = $pythonExePath
	$PythonVer = "Python $versionFound (installer)"
	Write-Host "Python installed to $installDir. Version: $versionFound"
	Write-Host "Installer log is at: $logPath"

	# Update PATH for this session
	$env:PATH = "$installDir;$installDir\Scripts;$env:PATH"

	# Update MACHINE‐level PATH
	Write-Host "Updating system PATH to include '$installDir' and '$installDir\Scripts'…"
	$machinePath = [Environment]::GetEnvironmentVariable("PATH",[EnvironmentVariableTarget]::Machine)
	if ($machinePath -notlike "*$installDir*") {
		$newMachinePath = "$machinePath;$installDir;$installDir\Scripts"
		[Environment]::SetEnvironmentVariable("PATH",$newMachinePath,[EnvironmentVariableTarget]::Machine)
		Write-Host "System PATH updated. You may need to restart or open a new session."
	} else {
		Write-Host "System PATH already contains Python paths. Skipping."
	}
}

# ──────────────────────────────────────────────────────────────────────────
if (-not (Test-Path $installDir)) {
	Write-Host "Creating install directory: $installDir …"
	New-Item -ItemType Directory -Path $installDir | Out-Null
}

Stop-Transcript
