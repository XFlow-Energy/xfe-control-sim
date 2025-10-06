<#
.SYNOPSIS
  Compile a Python script into a standalone Windows executable using PyInstaller.

.PARAMETER scriptPath
  Path to the Python script to compile (e.g. "plot_viewer.py").

.PARAMETER distDir
  Path to the directory where the executable will be placed (passed to PyInstaller's --distpath). Defaults to ".\dist".

.PARAMETER pythonInstallDir
  Root folder where Python is installed and venv lives (e.g. "C:\Program Files\XFEControlSimDeps\Python").

.EXAMPLE
  .\create_plot_viewer_executable.ps1 -scriptPath "plot_viewer.py" -distDir "C:\MyApp\bin" -pythonInstallDir "C:\Program Files\XFEControlSimDeps\Python"
#>

param(
	[string] $scriptPath          = "plot_viewer.py",
	[string] $distDir             = ".\dist",
	[string] $pythonInstallDir    = "C:\Program Files\XFEControlSimDeps\Python"
)

# Start logging into the same folder the script lives in
$logPath = Join-Path $PSScriptRoot 'install.log'
Start-Transcript -Path $logPath -Append

$ErrorActionPreference = 'Stop'

function Get-PyLauncher {
	<#
	.SYNOPSIS
	  Returns "py -3" if the Python launcher exists, otherwise $null.
	#>
	try {
		& py -3 --version > $null 2>&1
		return "py -3"
	} catch { return $null }
}

function Get-SystemPython {
	<#
	.SYNOPSIS
	  Returns "python" if it is a valid Python 3.x on PATH, otherwise $null.
	#>
	try {
		$ver = & python --version 2>&1
		if ($ver -match '^Python\s+3\.\d+\.\d+') {
			return "python"
		}
	} catch { }
	return $null
}

# ──────────────────────────────────────────────────────────────────────────
# Ensure distDir is absolute and exists
try {
	$distDir = (Resolve-Path -Path $distDir).Path
} catch {
	Write-Host "Output directory '$distDir' does not exist; creating…"
	New-Item -ItemType Directory -Path $distDir | Out-Null
	$distDir = (Resolve-Path -Path $distDir).Path
}

# venv setup
$VenvDir = Join-Path $pythonInstallDir "venv"
if (-not (Test-Path (Join-Path $VenvDir "Scripts\Activate.ps1"))) {
	Write-Host "Creating virtual environment at $VenvDir …"
	Write-Host "Checking if pip is installed…"
	try {
		& "$pythonInstallDir\python.exe" -m pip --version > $null 2>&1
		Write-Host "pip found."
	} catch {
		Write-Host "ERROR: pip not found. Ensure the installer included pip (Include_pip=1)." -ForegroundColor Red
		exit 1
	}
	Write-Host "Checking if virtualenv is installed…"
	try {
		& "$pythonInstallDir\python.exe" -m virtualenv --version > $null 2>&1
		Write-Host "virtualenv already installed."
	} catch {
		Write-Host "Installing virtualenv via pip…"
		& "$pythonInstallDir\python.exe" -m pip install virtualenv
	}
	Write-Host "Creating venv with virtualenv…"
	& "$pythonInstallDir\python.exe" -m virtualenv "$VenvDir"
} else {
	Write-Host "Virtual environment already exists at $VenvDir."
}
Write-Host "Activating virtual environment…"
. (Join-Path $VenvDir "Scripts\Activate.ps1")
Write-Host "Upgrading pip…"
try {
	& pip install --upgrade pip
} catch {
	Write-Host "WARNING: pip upgrade failed: $($_.Exception.Message)"
}
$RequiredPackages = @("pandas","pyqtgraph","PyQt5","pyinstaller")
Write-Host "Checking and installing required packages…"
foreach ($pkg in $RequiredPackages) {
	Write-Host "  → Checking $pkg …"
	try {
		& python -c "import $pkg" 2>$null
		Write-Host "    • $pkg already installed."
	} catch {
		Write-Host "    • Installing $pkg …"
		& pip install $pkg
	}
}

# ──────────────────────────────────────────────────────────────────────────
# Prefer the venv Python, otherwise fall back to launcher or system Python
$VenvPython = Join-Path $VenvDir "Scripts\python.exe"
if (Test-Path $VenvPython) {
	Write-Host "Using Python from venv: $VenvPython"
	$PythonExe = $VenvPython
} else {
	$PyLauncher = Get-PyLauncher
	$PythonExe  = if ($PyLauncher) { $PyLauncher } else { Get-SystemPython }
	if (-not $PythonExe) {
		Write-Host "ERROR: No Python 3 interpreter found. Install Python 3 and ensure 'py -3' or 'python' is on PATH." -ForegroundColor Red
		exit 1
	}
	Write-Host "Using Python command: $PythonExe"
}

# Verify the script exists
if (-not (Test-Path $scriptPath)) {
	Write-Host "ERROR: Script '$scriptPath' not found." -ForegroundColor Red
	exit 1
}

# 1) Ensure pip is available in the chosen interpreter
try {
	& $PythonExe -m pip --version > $null 2>&1
	Write-Host "pip is available in $PythonExe."
} catch {
	Write-Host "ERROR: pip is not available in $PythonExe. Install pip first." -ForegroundColor Red
	exit 1
}

# 2) Check for PyInstaller; install if missing under the chosen interpreter
try {
	& $PythonExe -m pyinstaller --version > $null 2>&1
	Write-Host "PyInstaller is already installed in $PythonExe environment."
} catch {
	Write-Host "PyInstaller not found. Installing via pip into $PythonExe..."
	& $PythonExe -m pip install pyinstaller
}

# Finally, run PyInstaller
Write-Host "Compiling '$scriptPath' into a standalone executable in '$distDir'…"
$PossiblePyInstaller = (Split-Path $PythonExe -Parent) + "\Scripts\pyinstaller.exe"
if (Test-Path $PossiblePyInstaller) {
	Write-Host "Using PyInstaller executable: $PossiblePyInstaller"
	& $PossiblePyInstaller --onefile `
						  --windowed `
						  --distpath $distDir `
						  $scriptPath
} else {
	Write-Host "Falling back to 'python -m PyInstaller'"
	& $PythonExe -m PyInstaller `
				  --onefile `
				  --windowed `
				  --distpath $distDir `
				  $scriptPath
}

if ($LASTEXITCODE -eq 0) {
	Write-Host "Compilation succeeded. Executable is in: $distDir" -ForegroundColor Green
} else {
	Write-Host "ERROR: PyInstaller reported an error." -ForegroundColor Red
	exit 1
}

Stop-Transcript
