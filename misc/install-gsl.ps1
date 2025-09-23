param(
    [switch]$Uninstall,
    [string]$tempDir     = "$env:TEMP\gsl_build",
    [string]$gslRepo     = "https://github.com/ampl/gsl.git",
    [string]$installDir  = "C:\gsl",
    [string]$cmakePath   = "C:/PROGRA~1/CMake/bin/cmake.exe",
    [string]$ninjaPath   = "C:/PROGRA~1/Ninja/ninja.exe",
    [string]$clangBin    = "C:\llvm-mingw\bin"
)

# Start logging into the same folder the script lives in
$logPath = Join-Path $PSScriptRoot 'install.log'
Start-Transcript -Path $logPath -Append

# Derived paths
$gslBuildDir = Join-Path $tempDir "build"
$jobs        = [Environment]::ProcessorCount

function Remove-FromMachinePath($folderToRemove) {
    $envName  = "PATH"
    $existing = [Environment]::GetEnvironmentVariable($envName, [EnvironmentVariableTarget]::Machine)
    if (-not $existing) { return }
    $newParts = $existing -split ";" | Where-Object { $_ -and ($_ -ne $folderToRemove) }
    $newValue = $newParts -join ";"
    [Environment]::SetEnvironmentVariable($envName, $newValue, [EnvironmentVariableTarget]::Machine)
}

# If -Uninstall, perform removal and exit
if ($Uninstall) {
    Write-Host "=== Uninstalling GSL ==="

    # 1) Verify elevation
    if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(`
        [Security.Principal.WindowsBuiltInRole] "Administrator")) {
        Write-Host "ERROR: Must run as Administrator to uninstall."
        exit 1
    }

    # 2) Remove PATH entries for bin, lib, include
    $binFolder     = Join-Path $installDir "bin"
    $libFolder     = Join-Path $installDir "lib"
    $includeFolder = Join-Path $installDir "include"

    Write-Host "Removing '$binFolder' from machine PATH…"
    Remove-FromMachinePath $binFolder
    Write-Host "Removing '$libFolder' from machine PATH…"
    Remove-FromMachinePath $libFolder
    Write-Host "Removing '$includeFolder' from machine PATH…"
    Remove-FromMachinePath $includeFolder

    # 3) Remove installation directory
    if (Test-Path $installDir) {
        Write-Host "Removing directory: $installDir"
        try {
            Remove-Item -Recurse -Force $installDir -ErrorAction Stop
            Write-Host "Deleted $installDir."
        } catch {
            Write-Host "ERROR: Could not remove $installDir."
            exit 1
        }
    } else {
        Write-Host "GSL installation not found at $installDir."
    }

    # 4) Remove temporary build directory
    if (Test-Path $tempDir) {
        Write-Host "Removing build directory: $tempDir"
        try {
            Remove-Item -Recurse -Force $tempDir -ErrorAction Stop
            Write-Host "Deleted $tempDir."
        } catch {
            Write-Host "WARNING: Could not delete $tempDir. Please remove it manually."
        }
    }

    # 5) Remove Uninstall registry key (32-bit & 64-bit)
    $scriptPath    = $MyInvocation.MyCommand.Path
    $uninstallKeys = @(
        "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\GSLPortable",
        "HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\GSLPortable"
    )
    foreach ($keyPath in $uninstallKeys) {
        if (Test-Path $keyPath) {
            Write-Host "Deleting registry key: $keyPath"
            try {
                Remove-Item -Path $keyPath -Recurse -Force -ErrorAction Stop
                Write-Host "Deleted registry key: $keyPath"
            } catch {
                Write-Host "WARNING: Could not delete registry key $keyPath."
            }
        }
    }

    Write-Host "GSL uninstalled successfully."
    Stop-Transcript
    exit 0
}

# ─────────────────────────────────────────────────────────────────
# Installation path (non-interactive)

# 1) Ensure elevation
if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(`
    [Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Host "ERROR: Must run as Administrator."
    exit 1
}

# 2) Prep PATH so clang, ninja, cmake are on it
function Ensure-BuildToolsInPath {
    $env:PATH = "$clangBin;$env:PATH"
    if (-not (Get-Command "clang.exe" -ErrorAction SilentlyContinue)) {
        Write-Host "ERROR: clang.exe not found in PATH."
        exit 1
    }
    if (-not (Test-Path $ninjaPath)) {
        Write-Host "ERROR: Ninja not found at $ninjaPath."
        exit 1
    }
    if (-not (Test-Path $cmakePath)) {
        Write-Host "ERROR: CMake not found at $cmakePath."
        exit 1
    }
    Write-Host "Build tools verified."
}

function Clone-GSLRepo {
    Write-Host "Cloning GSL repository from $gslRepo…"

    # Determine which git to use: prefer system git if available, otherwise use portable {app}\Git\cmd\git.exe
    $gitCmd = Get-Command git.exe -ErrorAction SilentlyContinue
    if ($gitCmd) {
        $gitExe = $gitCmd.Source
    } else {
        # Portable Git shipped under {app}\Git\cmd\git.exe (script lives in {app}\misc)
        $scriptRoot = $PSScriptRoot
        $appDir     = Split-Path $scriptRoot -Parent
        $gitExe     = Join-Path $appDir "Git\cmd\git.exe"
    }

	if (-not (Test-Path $gitExe)) {
		Write-Host "ERROR: git.exe not found at $gitExe"
		exit 1
	}

	# Remove any stale tempDir first
    if (Test-Path $tempDir) {
        try {
            Remove-Item -Recurse -Force $tempDir -ErrorAction Stop
        } catch {
            Write-Host "ERROR: Could not clear $tempDir. Check permissions."
            exit 1
        }
    }
    try {
        & $gitExe clone --depth 1 $gslRepo $tempDir 2>&1 | Write-Host
    } catch {
        Write-Host "ERROR: Failed to clone $gslRepo"
        exit 1
    }
    if (-not (Test-Path $tempDir)) {
        Write-Host "ERROR: Repository not found after clone."
        exit 1
    }
    Write-Host "Repository cloned successfully."
}

function Configure-Build {
    Write-Host "Configuring CMake (static only)…"
    if (Test-Path $gslBuildDir) {
        Remove-Item -Recurse -Force $gslBuildDir -ErrorAction SilentlyContinue
    }
    New-Item -Path $gslBuildDir -ItemType Directory | Out-Null

    try {
        & "$cmakePath" `
            -S "$tempDir" `
            -B "$gslBuildDir" `
            -G "Ninja" `
            -DBUILD_SHARED_LIBS=OFF `
            "-DCMAKE_POLICY_VERSION=3.12" `
            "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" `
            "-DCMAKE_INSTALL_PREFIX=$installDir" `
            "-DCMAKE_C_COMPILER=clang.exe" `
            "-DCMAKE_CXX_COMPILER=clang++.exe" `
            "-DCMAKE_MAKE_PROGRAM=$ninjaPath" `
            '-DCMAKE_C_FLAGS=-D_CRT_SECURE_NO_WARNINGS -Wno-enum-conversion -Wno-format -Wno-absolute-value -Wno-deprecated-declarations -Wno-inconsistent-dllimport -Wno-ignored-attributes -w' `
            '-DCMAKE_CXX_FLAGS=-D_CRT_SECURE_NO_WARNINGS -Wno-enum-conversion -Wno-format -Wno-absolute-value -Wno-deprecated-declarations -Wno-inconsistent-dllimport -Wno-ignored-attributes -w' `
            -DNO_AMPL_BINDINGS=1 `
            -DCMAKE_VERBOSE_MAKEFILE=ON `
            2>&1 | Write-Host
    } catch {
        Write-Host "ERROR: CMake configuration failed."
        exit 1
    }

    if (-not (Test-Path (Join-Path $gslBuildDir "build.ninja"))) {
        Write-Host "ERROR: CMake configure did not produce build.ninja."
        exit 1
    }
    Write-Host "CMake configuration completed."
}

function Build-GSL {
    Write-Host "Building GSL (static)…"
    try {
        & $ninjaPath -C $gslBuildDir -j $jobs 2>&1 | Write-Host
    } catch {
        Write-Host "ERROR: Build failed."
        exit 1
    }
    if (-not (Test-Path (Join-Path $gslBuildDir "libgsl.a"))) {
        Write-Host "ERROR: Build output missing: libgsl.a."
        exit 1
    }
    Write-Host "Build completed successfully."
}

function Install-GSL {
    Write-Host "Installing to $installDir…"
    try {
        & $cmakePath --install $gslBuildDir --prefix $installDir 2>&1 | Write-Host
    } catch {
        Write-Host "ERROR: Installation step failed."
        exit 1
    }

    $libFile    = Join-Path $installDir "lib\libgsl.a"
    $headerFile = Join-Path $installDir "include\gsl\gsl_math.h"
    if ((Test-Path $libFile) -and (Test-Path $headerFile)) {
        Write-Host "Installation completed successfully."
    } else {
        Write-Host "ERROR: Verification failed: missing $libFile or $headerFile."
        exit 1
    }
}

function Cleanup {
    Write-Host "Cleaning up $tempDir…"
    if (Test-Path $tempDir) {
        try {
            Remove-Item -Recurse -Force $tempDir -ErrorAction Stop
            Write-Host "Deleted $tempDir."
        } catch {
            Write-Host "WARNING: Could not delete $tempDir. Remove it manually."
        }
    }
}

# ─────────────────────────────────────────────────────────────────
# Remove old install if exists
if (Test-Path $installDir) {
    Write-Host "Existing GSL installation detected; removing…"
    try {
        Remove-Item -Recurse -Force $installDir -ErrorAction Stop
        Write-Host "Old installation removed."
    } catch {
        Write-Host "ERROR: Could not remove existing GSL."
        exit 1
    }
    # Clean old PATH entries
    Remove-FromMachinePath (Join-Path $installDir "bin")
    Remove-FromMachinePath (Join-Path $installDir "lib")
    Remove-FromMachinePath (Join-Path $installDir "include")
}

# 4) Verify build tools
Ensure-BuildToolsInPath

# 5) Clone source
Clone-GSLRepo

# 6) Configure
Configure-Build

# 7) Build & install
Build-GSL
Install-GSL

# 8) Cleanup
Cleanup

# 9) Update machine PATH to include bin, lib, include
$binFolder     = Join-Path $installDir "bin"
$libFolder     = Join-Path $installDir "lib"
$includeFolder = Join-Path $installDir "include"
$machinePath   = [Environment]::GetEnvironmentVariable("PATH", [EnvironmentVariableTarget]::Machine)

if ($machinePath -notlike "*$binFolder*") {
    [Environment]::SetEnvironmentVariable("PATH", "$machinePath;$binFolder", [EnvironmentVariableTarget]::Machine)
    Write-Host "Added '$binFolder' to PATH."
} else {
    Write-Host "'$binFolder' already in PATH."
}

$machinePath = [Environment]::GetEnvironmentVariable("PATH", [EnvironmentVariableTarget]::Machine)
if ($machinePath -notlike "*$libFolder*") {
    [Environment]::SetEnvironmentVariable("PATH", "$machinePath;$libFolder", [EnvironmentVariableTarget]::Machine)
    Write-Host "Added '$libFolder' to PATH."
} else {
    Write-Host "'$libFolder' already in PATH."
}

$machinePath = [Environment]::GetEnvironmentVariable("PATH", [EnvironmentVariableTarget]::Machine)
if ($machinePath -notlike "*$includeFolder*") {
    [Environment]::SetEnvironmentVariable("PATH", "$machinePath;$includeFolder", [EnvironmentVariableTarget]::Machine)
    Write-Host "Added '$includeFolder' to PATH."
} else {
    Write-Host "'$includeFolder' already in PATH."
}

Write-Host "You may need to restart or open a new session to see changes."

# 10) Register Uninstall key
Write-Host "Creating Uninstall registry key…"
$scriptPath    = $MyInvocation.MyCommand.Path
$uninstallKeys = @(
    "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\GSLPortable",
    "HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\GSLPortable"
)
foreach ($keyPath in $uninstallKeys) {
    if (-not (Test-Path $keyPath)) {
        New-Item -Path $keyPath -Force | Out-Null
    }
    Set-ItemProperty -Path $keyPath -Name "DisplayName"          -Value "GSL (Portable)"
    Set-ItemProperty -Path $keyPath -Name "DisplayVersion"       -Value "2.7"
    Set-ItemProperty -Path $keyPath -Name "Publisher"            -Value "GSL Project"
    Set-ItemProperty -Path $keyPath -Name "UninstallString"      `
        -Value "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$scriptPath`" -Uninstall"
    Set-ItemProperty -Path $keyPath -Name "QuietUninstallString" `
        -Value "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$scriptPath`" -Uninstall -quiet"
}

Write-Host "GSL has been successfully built and installed at $installDir."

Stop-Transcript

exit 0
