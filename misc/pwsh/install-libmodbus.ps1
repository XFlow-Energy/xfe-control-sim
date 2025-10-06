param(
    [switch]$Uninstall,
    [string]$installDir    = "C:\libmodbus",
    [string]$gitRepo       = "https://github.com/XFlow-Energy/libmodbus.git",
    [string]$gitBranch     = "cmake",
    [string]$llvmBin       = "C:/llvm-mingw/bin",
    [string]$cmakePath     = "C:/PROGRA~1/CMake/bin/cmake.exe",
    [string]$ninjaPath     = "C:/PROGRA~1/Ninja/ninja.exe"
)

# Start logging into the same folder the script lives in
$logPath = Join-Path $PSScriptRoot 'install.log'
Start-Transcript -Path $logPath -Append

# -------------------------
# Helper: Remove a folder from the machine-level PATH
function Remove-FromMachinePath($folderToRemove) {
    $envName  = "Path"
    $existing = [Environment]::GetEnvironmentVariable($envName, [System.EnvironmentVariableTarget]::Machine)
    if (-not $existing) { return }
    $newParts = $existing -split ";" | Where-Object { $_ -and ($_ -ne $folderToRemove) }
    $newValue = $newParts -join ";"
    [Environment]::SetEnvironmentVariable($envName, $newValue, [System.EnvironmentVariableTarget]::Machine)
}

# -------------------------
# If -Uninstall was passed, perform removal steps non-interactively
if ($Uninstall) {
    Write-Host "=== Uninstalling libmodbus from $installDir ==="

    # 1) Remove the installed folder (recursively)
    if (Test-Path $installDir) {
        Write-Host "Removing directory: $installDir"
        try {
            Remove-Item -Recurse -Force $installDir -ErrorAction Stop
            Write-Host "Deleted $installDir."
        } catch {
            Write-Host "ERROR: Could not remove $installDir. Ensure no files are in use and you have Administrator rights."
            exit 1
        }
    } else {
        Write-Host "Directory not found: $installDir (nothing to remove)"
    }

    # 2) Remove PATH entries (if added)
    $libFolder     = Join-Path $installDir "lib"
    $binFolder     = Join-Path $installDir "bin"
    $includeFolder = Join-Path $installDir "include"
    Write-Host "Removing '$libFolder' from machine PATH (if present)..."
    Remove-FromMachinePath $libFolder
    Write-Host "Removing '$binFolder' from machine PATH (if present)..."
    Remove-FromMachinePath $binFolder
    Write-Host "Removing '$includeFolder' from machine PATH (if present)..."
    Remove-FromMachinePath $includeFolder

    # 3) Remove Uninstall registry key
    $scriptPath    = $MyInvocation.MyCommand.Path
    $uninstallKeys = @(
        "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\LibmodbusPortable",
        "HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\LibmodbusPortable"
    )
    foreach ($keyPath in $uninstallKeys) {
        if (Test-Path $keyPath) {
            Write-Host "Deleting registry key: $keyPath"
            try {
                Remove-Item -Path $keyPath -Recurse -Force -ErrorAction Stop
                Write-Host "Deleted registry key: $keyPath"
            } catch {
                Write-Host "WARNING: Could not delete registry key $keyPath. You may remove it manually."
            }
        }
    }

    Write-Host "libmodbus has been uninstalled successfully."
    exit 0
}

# -------------------------
# Normal installation path

# 1) Ensure script is running as Administrator
if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(`
    [Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Host "ERROR: This script must be run as Administrator."
    exit 1
}

# 2) Derived values
$modbusBuildDir = Join-Path $installDir "build"
$jobs           = [Environment]::ProcessorCount

# Determine which git to use: prefer system git if available, otherwise use portable {app}\Git\cmd\git.exe
$gitCmd = Get-Command git.exe -ErrorAction SilentlyContinue
if ($gitCmd) {
    $gitExe    = $gitCmd.Source
    $gitBin    = Split-Path $gitExe
    # If using system git, we won’t have a separate usr\bin; just leave $gitUsrBin empty
    $gitUsrBin = ""
} else {
    # Portable Git shipped under {app}\Git\cmd\git.exe (script lives in {app}\misc)
    $scriptRoot = $PSScriptRoot
    $appDir     = Split-Path $scriptRoot -Parent
    $gitExe     = Join-Path $appDir "Git\cmd\git.exe"
    $gitBin     = Join-Path $appDir "Git\cmd"
    $gitUsrBin  = Join-Path $appDir "Git\usr\bin"
}

# 3) Detect if Ninja is available
if (Get-Command $ninjaPath -ErrorAction SilentlyContinue) {
    $useNinja = $true
} else {
    $useNinja = $false
}

# 4) Determine make.exe under llvm-mingw
if (Test-Path (Join-Path $llvmBin "mingw32-make.exe")) {
    $makeExe = Join-Path $llvmBin "mingw32-make.exe"
} elseif (Test-Path (Join-Path $llvmBin "make.exe")) {
    $makeExe = Join-Path $llvmBin "make.exe"
} else {
    Write-Host "ERROR: make executable not found in $llvmBin"
    exit 1
}

function Set-GnuTools {
    Write-Host "Adding llvm-mingw, Git, and Ninja folders to the PATH..."
    # Insert git / usr/bin / llvm-mingw / ninja into PATH
    $ninjaDir = Split-Path $ninjaPath
    $env:PATH = "$gitBin;$gitUsrBin;$llvmBin;$ninjaDir;$env:PATH"

    Write-Host "Verifying git version..."
    & $gitExe --version

    # Only attempt sed/grep if $gitUsrBin isn’t an empty string
    if ($gitUsrBin) {
        Write-Host "Verifying sed version..."
        & (Join-Path $gitUsrBin "sed.exe") --version

        Write-Host "Verifying grep version..."
        & (Join-Path $gitUsrBin "grep.exe") --version
    }

    Write-Host "Verifying clang version..."
    & (Join-Path $llvmBin "clang.exe") --version

    if ($useNinja) {
        Write-Host "Verifying ninja version..."
        & $ninjaPath --version
    } else {
        Write-Host "Verifying make version..."
        & $makeExe --version
    }
}

# 5) If $installDir already exists, remove it non-interactively (upgrade path)
if (Test-Path $installDir) {
    Write-Host "Detected existing libmodbus at $installDir. Removing for reinstall..."
    try {
        Remove-Item -Recurse -Force $installDir -ErrorAction Stop
        Write-Host "Old libmodbus installation removed."
    } catch {
        Write-Host "ERROR: Could not remove existing libmodbus. Check file locks or permissions."
        exit 1
    }
    # Also remove any PATH entries for the old install
    $oldLibFolder     = Join-Path $installDir "lib"
    $oldBinFolder     = Join-Path $installDir "bin"
    $oldIncludeFolder = Join-Path $installDir "include"
    Remove-FromMachinePath $oldLibFolder
    Remove-FromMachinePath $oldBinFolder
    Remove-FromMachinePath $oldIncludeFolder
}

# 6) Clone the repository
Write-Host "Cloning libmodbus fork (branch: $gitBranch) into $installDir..."
if (-not (Test-Path $gitExe)) {
    Write-Host "ERROR: git.exe not found at $gitExe"
    exit 1
}
try {
    & $gitExe clone --depth 1 --branch $gitBranch $gitRepo $installDir 2>&1 | Write-Host
} catch {
    Write-Host "ERROR: Failed to clone $gitRepo"
    exit 1
}

# 7) Ensure build tools are in PATH (now including Ninja)
Set-GnuTools

# 8) Configure with CMake
Write-Host "Configuring libmodbus with CMake..."
try {
    if (Test-Path $modbusBuildDir) {
        Remove-Item -Recurse -Force $modbusBuildDir -ErrorAction SilentlyContinue
    }
    New-Item -Path $modbusBuildDir -ItemType Directory -Force | Out-Null

    if ($useNinja) {
        $generator = "Ninja"
    } else {
        $generator = "MinGW Makefiles"
    }

    # Pass -DCMAKE_MAKE_PROGRAM explicitly when using Ninja
    if ($useNinja) {
        & $cmakePath `
            -S "$installDir" `
            -B "$modbusBuildDir" `
            -G $generator `
            -DCMAKE_MAKE_PROGRAM="$ninjaPath" `
            "-DCMAKE_POLICY_VERSION=3.12" `
            "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" `
            -DCMAKE_BUILD_TYPE=Release `
            -DBUILD_SHARED_LIBS=OFF `
            -DCMAKE_C_COMPILER="$llvmBin/clang.exe" `
            -DCMAKE_CXX_COMPILER="$llvmBin/clang++.exe" `
            -DCMAKE_INSTALL_PREFIX="$installDir" 2>&1 | Write-Host
    } else {
        & $cmakePath `
            -S "$installDir" `
            -B "$modbusBuildDir" `
            -G $generator `
            "-DCMAKE_POLICY_VERSION=3.12" `
            "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" `
            -DCMAKE_BUILD_TYPE=Release `
            -DBUILD_SHARED_LIBS=OFF `
            -DCMAKE_C_COMPILER="$llvmBin/clang.exe" `
            -DCMAKE_CXX_COMPILER="$llvmBin/clang++.exe" `
            -DCMAKE_INSTALL_PREFIX="$installDir" 2>&1 | Write-Host
    }
} catch {
    Write-Host "ERROR: CMake configuration failed."
    exit 1
}

# 9) Build
Write-Host "Building libmodbus..."
try {
    if ($useNinja) {
        & $ninjaPath -C "$modbusBuildDir" -j $jobs 2>&1 | Write-Host
    } else {
        & $makeExe -C "$modbusBuildDir" -j $jobs 2>&1 | Write-Host
    }
} catch {
    Write-Host "ERROR: Build failed."
    exit 1
}

# 10) Install
Write-Host "Installing libmodbus to $installDir..."
try {
    if ($useNinja) {
        & $ninjaPath -C "$modbusBuildDir" install 2>&1 | Write-Host
    } else {
        & $makeExe -C "$modbusBuildDir" install 2>&1 | Write-Host
    }
} catch {
    Write-Host "ERROR: Installation step failed."
    exit 1
}

# 11) Verify installation
$libFile   = Join-Path $installDir "lib\libmodbus.a"
$binFile   = Join-Path $installDir "bin\libmodbus.dll"
$hdrFolder = Join-Path $installDir "include\modbus\modbus.h"
if (-not (Test-Path $libFile)) {
    Write-Host "ERROR: Installation failed: $libFile is missing."
    exit 1
}
if (-not (Test-Path $binFile)) {
    Write-Host "ERROR: Installation failed: $binFile is missing."
    exit 1
}
if (-not (Test-Path $hdrFolder)) {
    Write-Host "ERROR: Installation failed: $hdrFolder is missing."
    exit 1
}

# ─────────────────────────────────────────────────────────────────
# 12) Update machine PATH to include both the new lib and include folders
Write-Host "Updating machine PATH to include libmodbus folders..."
$libFolder     = Join-Path $installDir "lib"
$binFolder     = Join-Path $installDir "bin"
$includeFolder = Join-Path $installDir "include"

if (-not ([System.Environment]::GetEnvironmentVariable("PATH", [System.EnvironmentVariableTarget]::Machine) `
            -like "*$libFolder*")) {
    $currentPath = [System.Environment]::GetEnvironmentVariable("PATH", [System.EnvironmentVariableTarget]::Machine)
    [System.Environment]::SetEnvironmentVariable("PATH", "$currentPath;$libFolder", `
        [System.EnvironmentVariableTarget]::Machine)
    Write-Host "Added '$libFolder' to PATH."
} else {
    Write-Host "libmodbus lib folder is already in PATH."
}

if (-not ([System.Environment]::GetEnvironmentVariable("PATH", [System.EnvironmentVariableTarget]::Machine) `
            -like "*$binFolder*")) {
    $currentPath = [System.Environment]::GetEnvironmentVariable("PATH", [System.EnvironmentVariableTarget]::Machine)
    [System.Environment]::SetEnvironmentVariable("PATH", "$currentPath;$binFolder", `
        [System.EnvironmentVariableTarget]::Machine)
    Write-Host "Added '$binFolder' to PATH."
} else {
    Write-Host "libmodbus bin folder is already in PATH."
}

if (-not ([System.Environment]::GetEnvironmentVariable("PATH", [System.EnvironmentVariableTarget]::Machine) `
            -like "*$includeFolder*")) {
    $currentPath = [System.Environment]::GetEnvironmentVariable("PATH", [System.EnvironmentVariableTarget]::Machine)
    [System.Environment]::SetEnvironmentVariable("PATH", "$currentPath;$includeFolder", `
        [System.EnvironmentVariableTarget]::Machine)
    Write-Host "Added '$includeFolder' to PATH."
} else {
    Write-Host "libmodbus include folder is already in PATH."
}

Write-Host "You may need to restart or open a new session to see changes."

# 13) Register our Uninstall key so that Control Panel (or Inno Setup) can call us with -Uninstall
Write-Host "Creating Uninstall registry key..."
$scriptPath    = $MyInvocation.MyCommand.Path
$uninstallKeys = @(
    "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\LibmodbusPortable",
    "HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\LibmodbusPortable"
)
foreach ($keyPath in $uninstallKeys) {
    if (-not (Test-Path $keyPath)) {
        New-Item -Path $keyPath -Force | Out-Null
    }
    Set-ItemProperty -Path $keyPath -Name "DisplayName"          -Value "libmodbus (portable)"
    Set-ItemProperty -Path $keyPath -Name "DisplayVersion"       -Value "1.0"
    Set-ItemProperty -Path $keyPath -Name "Publisher"            -Value "XFlow-Energy"
    Set-ItemProperty -Path $keyPath -Name "UninstallString"      `
        -Value "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$scriptPath`" -Uninstall"
    Set-ItemProperty -Path $keyPath -Name "QuietUninstallString" `
        -Value "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$scriptPath`" -Uninstall -quiet"
}

Write-Host "libmodbus installed successfully!"

Stop-Transcript

exit 0
