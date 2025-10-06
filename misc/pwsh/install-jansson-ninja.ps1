<#
.SYNOPSIS
  Download and build the latest Jansson release (static-only) using CMake + Ninja,
  installing into a custom folder.  Supports a 100% silent, non‐MSI approach by
  defaulting to the GitHub “latest” release.

.PARAMETER installDir
  The directory into which to install Jansson (e.g. "C:\Program Files\Jansson").

.PARAMETER janssonZip
  (Optional) If you already know a direct download URL for a specific Jansson ZIP
  (e.g. "https://github.com/akheron/jansson/archive/refs/tags/v2.14.zip"), pass it here.
  Otherwise the script queries GitHub’s API and constructs the “latest tag” ZIP URL.

.PARAMETER Uninstall
  If passed, attempt to remove Jansson by deleting the folder, cleaning up the build
  directory, removing any PATH entries, and deleting the “portable” registry keys.

.EXAMPLE
  # Install the latest automatically:
  .\install-jansson-ninja.ps1 -installDir "C:\Program Files\Jansson" `
    -clangBin "C:\llvm-mingw\bin" `
    -cmakePath "C:\Program Files\CMake\bin\cmake.exe" `
    -ninjaPath "C:\Program Files\Ninja\ninja.exe"

  # Uninstall:
  .\install-jansson-ninja.ps1 -Uninstall -installDir "C:\Program Files\Jansson"

  # Or explicitly install from a known URL:
  .\install-jansson-ninja.ps1 -installDir "C:\Program Files\Jansson" `
    -janssonZip "https://github.com/akheron/jansson/archive/refs/tags/v2.15.zip"
#>

param(
    [switch]$Uninstall,

    # If empty, we will query GitHub for the latest release and build the URL ourselves.
    [string]$janssonZip    = "",

    [string]$tempDir       = "$env:TEMP\jansson_build",
    [string]$installDir    = "C:\Program Files\Jansson",
    [string]$clangBin      = "C:\llvm-mingw\bin",
    [string]$cmakePath     = "C:/Program Files/CMake/bin/cmake.exe",
    [string]$ninjaPath     = "C:/Program Files/Ninja/ninja.exe"
)

# Start logging into the same folder the script lives in
$logPath = Join-Path $PSScriptRoot 'install.log'
Start-Transcript -Path $logPath -Append

# Derived values
$jobs     = [Environment]::ProcessorCount
$buildDir = Join-Path $tempDir "build"

# Helper: Remove a single folder from the machine-level PATH
function Remove-FromMachinePath($folderToRemove) {
    $envName  = "PATH"
    $existing = [Environment]::GetEnvironmentVariable($envName, [System.EnvironmentVariableTarget]::Machine)
    if (-not $existing) { return }
    $newParts = $existing -split ";" | Where-Object { $_ -and ($_ -ne $folderToRemove) }
    $newValue = $newParts -join ";"
    [Environment]::SetEnvironmentVariable($envName, $newValue, [System.EnvironmentVariableTarget]::Machine)
}

# ─────────────────────────────────────────────────────────────────
if ($Uninstall) {
    Write-Host "=== Uninstalling Jansson ==="

    # 1) Ensure we are elevated
    if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(`
        [Security.Principal.WindowsBuiltInRole] "Administrator")) {
        Write-Host "ERROR: Must run as Administrator to uninstall."
        exit 1
    }

    # 2) Remove installation directory
    if (Test-Path $installDir) {
        Write-Host "Removing directory: $installDir"
        try {
            Remove-Item -Recurse -Force $installDir -ErrorAction Stop
            Write-Host "Deleted $installDir."
        } catch {
            Write-Host "ERROR: Could not remove $installDir. Check permissions or file locks."
            exit 1
        }
    } else {
        Write-Host "Jansson not found at $installDir."
    }

    # 2) Remove PATH entries (if added)
    $libFolder     = Join-Path $installDir "lib"
    $includeFolder = Join-Path $installDir "include"
    Write-Host "Removing '$libFolder' from machine PATH (if present)..."
    Remove-FromMachinePath $libFolder
    Write-Host "Removing '$includeFolder' from machine PATH (if present)..."
    Remove-FromMachinePath $includeFolder

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

    # 5) Remove Uninstall registry key (32-bit and 64-bit)
    $scriptPath    = $MyInvocation.MyCommand.Path
    $uninstallKeys = @(
        "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\JanssonPortable",
        "HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\JanssonPortable"
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

    Write-Host "Jansson uninstalled successfully."
    exit 0
}

# ─────────────────────────────────────────────────────────────────
# Installation logic

# 1) Ensure we are elevated
if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(`
    [Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Host "ERROR: Must run as Administrator."
    exit 1
}

# 2) If the user did NOT supply a direct $janssonZip, query GitHub API for the latest tag
if (-not $janssonZip) {
    Write-Host "Fetching latest Jansson release information from GitHub..."
    try {
        $release = Invoke-RestMethod -Uri "https://api.github.com/repos/akheron/jansson/releases/latest" `
                                     -UseBasicParsing -ErrorAction Stop

        # The Jansson repo’s “release” assets may include source zips, but if none are listed,
        # we construct the source-archive URL from the tag name.  Most often, Jansson’s releases
        # are tags, so $release.tag_name is e.g. "v2.14".
        $tag = $release.tag_name
        if (-not $tag) {
            Write-Host "ERROR: Could not determine latest Jansson tag from GitHub response."
            exit 1
        }

        # Construct the official “source zip” URL:
        $janssonZip = "https://github.com/akheron/jansson/archive/refs/tags/$tag.zip"
        $versionString = $tag.TrimStart("v")
        Write-Host "→ Latest Jansson tag is '$tag' → ZIP URL = $janssonZip"
    } catch {
        Write-Host "ERROR: Failed to fetch latest Jansson release info: $_"
        exit 1
    }
} else {
    # If the user provided a specific URL, attempt to extract a version from it (e.g. v2.14)
    Write-Host "Using user-supplied Jansson ZIP URL: $janssonZip"
    if ($janssonZip -match "/v?([\d\.]+)\.zip$") {
        $versionString = $Matches[1]
    } else {
        $versionString = "custom"
    }
}

function Ensure-BuildTools {
    # Add clangBin to PATH and verify clang, Ninja, CMake
    $env:PATH = "$clangBin;$env:PATH"
    if (-not (Get-Command clang.exe -ErrorAction SilentlyContinue)) {
        Write-Host "ERROR: clang.exe not found in PATH."
        exit 1
    }
    if (-not (Get-Command $ninjaPath -ErrorAction SilentlyContinue)) {
        Write-Host "ERROR: Ninja not found at $ninjaPath."
        exit 1
    }
    if (-not (Get-Command $cmakePath -ErrorAction SilentlyContinue)) {
        Write-Host "ERROR: CMake not found at $cmakePath."
        exit 1
    }
    Write-Host "Build tools verified."
}

function Fetch-And-Extract {
    Write-Host "Fetching Jansson from $janssonZip..."
    if (Test-Path $tempDir) {
        Remove-Item -Recurse -Force $tempDir -ErrorAction SilentlyContinue
    }
    New-Item -Path $tempDir -ItemType Directory | Out-Null

    $zipFile = Join-Path $tempDir "jansson.zip"
    try {
        Invoke-WebRequest -Uri $janssonZip -OutFile $zipFile -UseBasicParsing -ErrorAction Stop
    } catch {
        Write-Host "ERROR: Failed to download $janssonZip."
        exit 1
    }

    try {
        Expand-Archive -Path $zipFile -DestinationPath $tempDir -Force
    } catch {
        Write-Host "ERROR: Failed to extract $zipFile."
        exit 1
    }

    # Locate extracted source directory (the first subfolder)
    $srcDir = Get-ChildItem $tempDir -Directory | Select-Object -First 1 | ForEach-Object { $_.FullName }
    if (-not (Test-Path (Join-Path $srcDir "CMakeLists.txt"))) {
        Write-Host "ERROR: CMakeLists.txt not found in $srcDir."
        exit 1
    }
    return $srcDir
}

function Configure-Build {
    param($srcDir)
    Write-Host "Configuring static build (Jansson v$versionString)..."
    if (Test-Path $buildDir) {
        Remove-Item -Recurse -Force $buildDir -ErrorAction SilentlyContinue
    }
    New-Item -Path $buildDir -ItemType Directory | Out-Null

    try {
        & "$cmakePath" `
            -S "$srcDir" `
            -B "$buildDir" `
            -G "Ninja" `
            -DJANSSON_BUILD_SHARED_LIBS=OFF `
            "-DCMAKE_MAKE_PROGRAM=$ninjaPath" `
            "-DCMAKE_POLICY_VERSION=3.12" `
            "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" `
            "-DCMAKE_INSTALL_PREFIX=$installDir" `
            "-DCMAKE_C_COMPILER=clang.exe" `
            "-DCMAKE_CXX_COMPILER=clang++.exe" `
            2>&1 | Write-Host
    } catch {
        Write-Host "ERROR: CMake configuration failed."
        exit 1
    }

    if (-not (Test-Path (Join-Path $buildDir "build.ninja"))) {
        Write-Host "ERROR: CMake configure did not produce build.ninja."
        exit 1
    }
    Write-Host "CMake configuration completed."
}

function Build-And-Install {
    Write-Host "Building Jansson v$versionString..."
    try {
        & $ninjaPath -C $buildDir -j $jobs 2>&1 | Write-Host
    } catch {
        Write-Host "ERROR: Build step failed."
        exit 1
    }

    Write-Host "Installing to $installDir..."
    try {
        & $cmakePath --install $buildDir --prefix $installDir 2>&1 | Write-Host
    } catch {
        Write-Host "ERROR: Installation step failed."
        exit 1
    }

    # Verify
    $lib = Join-Path $installDir "lib\libjansson.a"
    $hdr = Join-Path $installDir "include\jansson.h"
    if ((-not (Test-Path $lib)) -or (-not (Test-Path $hdr))) {
        Write-Host "ERROR: Verification failed: missing $lib or $hdr."
        exit 1
    }
    Write-Host "Jansson static library installed to $installDir."
}

function Cleanup {
    Write-Host "Cleaning up $tempDir..."
    if (Test-Path $tempDir) {
        try {
            Remove-Item -Recurse -Force $tempDir -ErrorAction Stop
            Write-Host "Deleted $tempDir."
        } catch {
            Write-Host "WARNING: Could not delete $tempDir. Please remove it manually."
        }
    }
}

# ─────────────────────────────────────────────────────────────────
# If Jansson is already installed, remove it (upgrade path)
if (Test-Path $installDir) {
    Write-Host "Detected existing Jansson at $installDir. Removing for reinstall..."
    try {
        Remove-Item -Recurse -Force $installDir -ErrorAction Stop
        Write-Host "Old Jansson installation removed."
    } catch {
        Write-Host "ERROR: Could not remove existing Jansson. Check permissions or file locks."
        exit 1
    }

    $oldLibFolder     = Join-Path $installDir "lib"
    $oldIncludeFolder = Join-Path $installDir "include"
    Remove-FromMachinePath $oldLibFolder
    Remove-FromMachinePath $oldIncludeFolder
}

# 3) Ensure required build tools exist
Ensure-BuildTools

# 4) Download and extract source
$source = Fetch-And-Extract

# 5) Configure
Configure-Build -srcDir $source

# 6) Build & Install
Build-And-Install

# 7) Cleanup temporary build dir
Cleanup

# 8) Update machine PATH if Jansson installed tools into bin (unlikely)
Write-Host "Updating machine PATH to include jansson folders..."
$libFolder     = Join-Path $installDir "lib"
$includeFolder = Join-Path $installDir "include"

if (-not ([System.Environment]::GetEnvironmentVariable("PATH", [System.EnvironmentVariableTarget]::Machine) `
            -like "*$libFolder*")) {
    $currentPath = [System.Environment]::GetEnvironmentVariable("PATH", [System.EnvironmentVariableTarget]::Machine)
    [System.Environment]::SetEnvironmentVariable("PATH", "$currentPath;$libFolder", `
        [System.EnvironmentVariableTarget]::Machine)
    Write-Host "Added '$libFolder' to PATH."
} else {
    Write-Host "jansson lib folder is already in PATH."
}

if (-not ([System.Environment]::GetEnvironmentVariable("PATH", [System.EnvironmentVariableTarget]::Machine) `
            -like "*$includeFolder*")) {
    $currentPath = [System.Environment]::GetEnvironmentVariable("PATH", [System.EnvironmentVariableTarget]::Machine)
    [System.Environment]::SetEnvironmentVariable("PATH", "$currentPath;$includeFolder", `
        [System.EnvironmentVariableTarget]::Machine)
    Write-Host "Added '$includeFolder' to PATH."
} else {
    Write-Host "jansson include folder is already in PATH."
}
# 9) Register Uninstall key (32-bit & 64-bit)
Write-Host "Creating Uninstall registry key (Jansson v$versionString)..."
$scriptPath    = $MyInvocation.MyCommand.Path
$uninstallKeys = @(
    "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\JanssonPortable",
    "HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\JanssonPortable"
)
foreach ($keyPath in $uninstallKeys) {
    if (-not (Test-Path $keyPath)) {
        New-Item -Path $keyPath -Force | Out-Null
    }
    Set-ItemProperty -Path $keyPath -Name "DisplayName"          -Value "Jansson (Portable)"
    Set-ItemProperty -Path $keyPath -Name "DisplayVersion"       -Value $versionString
    Set-ItemProperty -Path $keyPath -Name "Publisher"            -Value "Jansson Project"
    Set-ItemProperty -Path $keyPath -Name "UninstallString"      `
        -Value "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$scriptPath`" -Uninstall"
    Set-ItemProperty -Path $keyPath -Name "QuietUninstallString" `
        -Value "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$scriptPath`" -Uninstall -quiet"
}

Write-Host "Jansson has been successfully built and installed at $installDir (v$versionString)."

Stop-Transcript

exit 0
