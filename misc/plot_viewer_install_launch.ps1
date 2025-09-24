# Requires Windows PowerShell 5.0+ or PowerShell Core

$ErrorActionPreference = 'Stop'

Write-Host "Detecting platform..."
if ($env:OS -ne "Windows_NT") {
	Write-Error "This script is intended for Windows. Use the shell script on Linux/macOS."
	exit 1
}
Write-Host "OS Detected: Windows"

function Get-PythonInfo {
	# Returns @($exeName, $versionString) or @($null, $null) if not valid
	$pythonExe = $null
	$verArg    = ""

	if (Get-Command py -ErrorAction SilentlyContinue) {
		$pythonExe = "py"
		$verArg    = "-3"
	} elseif (Get-Command python -ErrorAction SilentlyContinue) {
		$pythonExe = "python"
		$verArg    = ""
	} else {
		return @($null, $null)
	}

	# Attempt to get version output
	try {
		if ($verArg) {
			$versionOutput = & $pythonExe $verArg --version 2>&1
		} else {
			$versionOutput = & $pythonExe --version 2>&1
		}
	} catch {
		return @($null, $null)
	}

	# Validate version string format: "Python X.Y.Z"
	if ($versionOutput -match '^Python\s+\d+\.\d+\.\d+') {
		return @($pythonExe, $versionOutput.Trim())
	} else {
		return @($null, $null)
	}
}

# Check for a real Python installation
$pyInfo = Get-PythonInfo
$PythonExe  = $pyInfo[0]
$PythonVer  = $pyInfo[1]

if (-not $PythonExe) {
	Write-Host "Python not found. Downloading installer..."
	$installerUrl  = "https://www.python.org/ftp/python/3.11.4/python-3.11.4-amd64.exe"
	$installerPath = "$PSScriptRoot\python-installer.exe"

	Invoke-WebRequest -Uri $installerUrl -OutFile $installerPath

	Write-Host "Installing Python silently (may require admin privileges)..."
	Start-Process -FilePath $installerPath -ArgumentList "/quiet InstallAllUsers=1 PrependPath=1" -Wait

	Remove-Item $installerPath -Force

	Write-Host "Python installation complete. Refreshing PATH..."
	$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine")

	# Re-check after installation
	$pyInfo    = Get-PythonInfo
	$PythonExe = $pyInfo[0]
	$PythonVer = $pyInfo[1]

	if (-not $PythonExe) {
		Write-Error "Python was installed but still not found. Please restart your terminal or add Python to PATH."
		exit 1
	}
}

Write-Host "Using Python: $PythonExe ($PythonVer)"

# Verify Python can run commands
try {
	if ($PythonVer -match 'Python\s+3') {
		if ($PythonExe -eq "py") {
			& $PythonExe -3 --version | Out-Null
		} else {
			& $PythonExe --version | Out-Null
		}
	} else {
		Write-Error "Detected Python version is not 3.x. Please install Python 3."
		exit 1
	}
	Write-Host "Python is operational."
} catch {
	Write-Error "Python command failed. Ensure Python 3 is correctly installed."
	exit 1
}

# Put .venv at C:\
$VenvDir = "C:\.venv"

# Create virtual environment if it doesn't exist
if (-not (Test-Path "$VenvDir\Scripts\Activate.ps1")) {
	Write-Host "Creating virtual environment in $VenvDir"
	if ($PythonExe -eq "py") {
		& $PythonExe -3 -m venv $VenvDir
	} else {
		& $PythonExe -m venv $VenvDir
	}
} else {
	Write-Host "Virtual environment already exists at C:\.venv"
}

if (-not (Test-Path "$VenvDir\Scripts\Activate.ps1")) {
	Write-Error "Failed to create virtual environment. Cannot find Activate.ps1."
	exit 1
}

Write-Host "Activating virtual environment..."
. "$VenvDir\Scripts\Activate.ps1"

Write-Host "Upgrading pip..."
& pip install --upgrade pip

$RequiredPackages = @("pandas", "pyqtgraph", "PyQt5")

Write-Host "Checking and installing required packages..."
for ($i = 0; $i -lt $RequiredPackages.Count; $i++) {
	$pkg = $RequiredPackages[$i]
	Write-Host "Checking $pkg..."
	try {
		& python -c "import $pkg" 2>$null
		if ($LASTEXITCODE -eq 0) {
			Write-Host " • $pkg already installed."
		} else {
			throw
		}
	} catch {
		Write-Host " • Installing $pkg..."
		& pip install $pkg
	}
}

# Verify main script exists next to this file
$ScriptDir = $PSScriptRoot
Push-Location $ScriptDir
if (-not (Test-Path "plot_viewer.py")) {
	Write-Error "plot_viewer.py not found in $ScriptDir."
	Pop-Location
	exit 1
}

Write-Host "Launching plot_viewer.py..."
& python "$ScriptDir\plot_viewer.py"
Pop-Location
deactivate
