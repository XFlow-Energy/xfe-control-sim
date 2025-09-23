<#
.SYNOPSIS
  Download and “portable‐style” install the latest CMake ZIP into a custom folder,
  then verify that cmake.exe is in place.  Supports a 100% silent, non‐MSI approach.

.PARAMETER installDir
  The directory into which to install CMake (e.g. "C:\Program Files\XFEControlSimDeps\CMake").

.PARAMETER cmakeZipUrl
  (Optional) If you already know a direct download URL for a specific CMake ZIP,
  pass it here. Otherwise, the script queries GitHub’s API for the latest release
  and picks the Windows x86_64 ZIP automatically.

.PARAMETER Uninstall
  If passed, attempt to remove CMake by deleting the folder, removing any PATH entries, and
  cleaning up the “portable” registry key.

.EXAMPLE
  # Install the latest automatically:
  .\install-cmake-zip.ps1 -installDir "C:\Program Files\XFEControlSimDeps\CMake"

  # Specify a custom URL (not usually needed once script auto‐discovers latest):
  .\install-cmake-zip.ps1 -installDir "C:\Program Files\XFEControlSimDeps\CMake" `
    -cmakeZipUrl "https://github.com/Kitware/CMake/releases/download/v4.0.3/cmake-4.0.3-windows-x86_64.zip"

  # Uninstall:
  .\install-cmake-zip.ps1 -Uninstall -installDir "C:\Program Files\XFEControlSimDeps\CMake"
#>

param(
    [switch]$Uninstall,

    [string]$installDir       = "C:\Program Files\XFEControlSimDeps\CMake",

    [string]$cmakeZipUrl      = "",

    [string]$tempZipPath      = ""
)

# Start logging into the same folder the script lives in
$logPath = Join-Path $PSScriptRoot 'install.log'
Start-Transcript -Path $logPath -Append

# ──────────────────────────────────────────────────────────────────────────
# Derived paths
$binDir    = Join-Path $installDir "bin"
$cmakeExe  = Join-Path $binDir "cmake.exe"
$portableRegPaths = @(
    "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\CMakePortable",
    "HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\CMakePortable"
)

function Remove-FromMachinePath($folderToRemove) {
    $envName  = "PATH"
    $existing = [Environment]::GetEnvironmentVariable($envName, [EnvironmentVariableTarget]::Machine)
    if (-not $existing) { return }
    $newParts = $existing -split ";" | Where-Object { $_ -and ($_ -ne $folderToRemove) }
    $newValue = $newParts -join ";"
    [Environment]::SetEnvironmentVariable($envName, $newValue, [EnvironmentVariableTarget]::Machine)
}

function Get-LatestCMakeRelease {
    <#
    .SYNOPSIS
      Queries GitHub API to find the latest CMake Windows x86_64 ZIP URL and version.
    .OUTPUTS
      An object with .Url and .VersionString fields.
    #>
    try {
        $release = Invoke-RestMethod -Uri "https://api.github.com/repos/Kitware/CMake/releases/latest" `
                                     -UseBasicParsing -ErrorAction Stop
    } catch {
        Write-Host "ERROR: Failed to query GitHub API for latest CMake release: $_"
        exit 1
    }

    $tag = $release.tag_name
    if (-not $tag) {
        Write-Host "ERROR: Could not determine latest CMake tag from GitHub response."
        exit 1
    }
    # Find an asset whose name ends with "windows-x86_64.zip"
    $asset = $release.assets | Where-Object { $_.name -match "windows-x86_64\.zip$" } | Select-Object -First 1
    if (-not $asset) {
        Write-Host "ERROR: Could not find a Windows x86_64 ZIP asset in the latest CMake release."
        exit 1
    }
    return @{
        Url = $asset.browser_download_url
        VersionString = $tag.TrimStart("v")
    }
}

# ──────────────────────────────────────────────────────────────────────────
# UNINSTALL LOGIC
if ($Uninstall) {
    Write-Host "=== Uninstalling CMake (ZIP version) ===`n"

    # 1) Ensure we are elevated
    if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(`
         [Security.Principal.WindowsBuiltInRole] "Administrator")) {
        Write-Host "ERROR: Must run as Administrator to uninstall."
        exit 1
    }

    # 2) Remove $binDir from PATH (even if it no longer exists on disk)
    Write-Host "Removing '$binDir' from Machine PATH (if present)…"
    Remove-FromMachinePath $binDir

    # 3) Remove the install directory if it exists
    if (Test-Path $installDir) {
        Write-Host "`nRemoving installation directory: $installDir"
        try {
            Remove-Item -Recurse -Force $installDir -ErrorAction Stop
            Write-Host "Deleted $installDir."
        } catch {
            Write-Host "ERROR: Could not remove $installDir. Check permissions or file locks."
            exit 1
        }
    } else {
        Write-Host "No CMake folder found at $installDir."
    }

    # 4) Delete our “portable” Uninstall registry keys (32‐bit & 64‐bit)
    foreach ($rkey in $portableRegPaths) {
        if (Test-Path $rkey) {
            Write-Host "`nDeleting registry key: $rkey"
            try {
                Remove-Item -Path $rkey -Recurse -Force -ErrorAction Stop
                Write-Host "Deleted registry key: $rkey"
            } catch {
                Write-Host "WARNING: Could not delete registry key $rkey."
            }
        }
    }

    Write-Host "`nCMake (ZIP) uninstalled successfully."
    exit 0
}

# ──────────────────────────────────────────────────────────────────────────
# INSTALLATION LOGIC

# 1) Ensure elevation
if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(`
     [Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Host "ERROR: Must run as Administrator."
    exit 1
}

# 2) Determine the ZIP URL and version
if (-not $cmakeZipUrl) {
    Write-Host "Auto-detecting latest CMake release..."
    $info = Get-LatestCMakeRelease
    $cmakeZipUrl   = $info.Url
    $versionString = $info.VersionString
    Write-Host "→ Latest CMake version: $versionString"
    Write-Host "→ Download URL: $cmakeZipUrl"
} else {
    # User provided a specific URL; try to parse version from it
    if ($cmakeZipUrl -match "/v?([\d\.]+)-windows-x86_64\.zip$") {
        $versionString = $Matches[1]
    } else {
        $versionString = "custom"
    }
    Write-Host "Using user-supplied CMake ZIP URL: $cmakeZipUrl (version: $versionString)"
}

# 3) Build a temp-zip path if not provided
if (-not $tempZipPath) {
    $zipName = Split-Path $cmakeZipUrl -Leaf
    $tempZipPath = Join-Path $env:TEMP $zipName
}

# 4) If CMake is already present, offer to remove and re-install
$existing = Get-ChildItem -Path $installDir -Recurse -Filter cmake.exe -ErrorAction SilentlyContinue | Select-Object -First 1
if ($existing) {
    Write-Host "CMake appears already installed at: $($existing.FullName)"
    $choice = Read-Host "Do you want to remove and re-install? (y/n)"
    if ($choice -eq "y") {
        Write-Host "`nRemoving existing CMake installation..."
        try {
            Remove-Item -Recurse -Force $installDir -ErrorAction Stop
            Write-Host "Deleted old folder: $installDir"
            Remove-FromMachinePath $binDir
        } catch {
            Write-Host "ERROR: Unable to delete $installDir. Check permissions."
            exit 1
        }
    } else {
        Write-Host "Installation cancelled."
        exit 0
    }
}

# 5) Download the ZIP into $tempZipPath
Write-Host "`nDownloading CMake ZIP from $cmakeZipUrl …"
try {
    Invoke-WebRequest -Uri $cmakeZipUrl -OutFile $tempZipPath -UseBasicParsing -ErrorAction Stop
    Write-Host "Download complete: $tempZipPath"
} catch {
    Write-Host "ERROR: Failed to download $cmakeZipUrl"
    exit 1
}

# 6) Verify the ZIP exists
if (-not (Test-Path $tempZipPath)) {
    Write-Host "ERROR: Downloaded file not found at $tempZipPath"
    exit 1
}

# 7) Extract the ZIP into a temporary folder
$extractRoot = Join-Path $env:TEMP "cmake-zip-extract"
if (Test-Path $extractRoot) {
    Remove-Item -Recurse -Force $extractRoot
}
New-Item -ItemType Directory -Path $extractRoot | Out-Null

Write-Host "`nExtracting ZIP to $extractRoot …"
try {
    Expand-Archive -Path $tempZipPath -DestinationPath $extractRoot -Force
    Write-Host "Extraction complete."
} catch {
    Write-Host "ERROR: Failed to extract ZIP. $_"
    exit 1
}

# 8) Locate the internal “bin” folder.  By convention, the ZIP unpacks to:
#       <extractRoot>\cmake-<version>-windows-x86_64\bin
Write-Host "`nLocating cmake.exe inside extracted files…"
$found = Get-ChildItem -Path $extractRoot -Recurse -Filter cmake.exe -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $found) {
    Write-Host "ERROR: cmake.exe not found in extracted ZIP."
    exit 1
}
$actualBinDir = Split-Path -Parent $found.FullName
Write-Host "→ Found cmake.exe at: $($found.FullName)"
Write-Host "→ Using bin folder: $actualBinDir"

# 9) Now move everything under that “root of CMake” into $installDir
$cmakeRoot = Split-Path -Parent $actualBinDir
Write-Host "`nCopying CMake files to `$installDir` …"
try {
    if (-not (Test-Path $installDir)) {
        New-Item -ItemType Directory -Path $installDir | Out-Null
    }
    Copy-Item -Path (Join-Path $cmakeRoot "*") -Destination $installDir -Recurse -Force
    Write-Host "Copied all files to $installDir."
} catch {
    Write-Host "ERROR: Could not copy CMake files to $installDir. $_"
    exit 1
}

# 10) Clean up the ZIP and extraction folder
Write-Host "`nCleaning up temporary files…"
try {
    Remove-Item -Path $tempZipPath -Force -ErrorAction SilentlyContinue
    Remove-Item -Path $extractRoot -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "Deleted $tempZipPath and $extractRoot."
} catch {
    Write-Host "WARNING: Could not fully clean up $tempZipPath or $extractRoot."
}

# 11) Update machine PATH to include the new bin
Write-Host "`nUpdating system PATH to include '$binDir' …"
$machinePath = [Environment]::GetEnvironmentVariable("PATH", [EnvironmentVariableTarget]::Machine)
if ($machinePath -notlike "*$binDir*") {
    [Environment]::SetEnvironmentVariable("PATH", "$machinePath;$binDir", [System.EnvironmentVariableTarget]::Machine)
    Write-Host "System PATH updated. You will need to restart or open a new session."
} else {
    Write-Host "PATH already contains $binDir. Skipping."
}

# 12) Create (or update) our portable “Uninstall” registry entries
Write-Host "`nCreating portable Uninstall registry keys …"
$scriptPath = $MyInvocation.MyCommand.Path
foreach ($rkey in $portableRegPaths) {
    if (-not (Test-Path $rkey)) {
        New-Item -Path $rkey -Force | Out-Null
    }
    Set-ItemProperty -Path $rkey -Name "DisplayName"          -Value "CMake (Portable ZIP)"
    Set-ItemProperty -Path $rkey -Name "DisplayVersion"       -Value $versionString
    Set-ItemProperty -Path $rkey -Name "Publisher"            -Value "Kitware"
    Set-ItemProperty -Path $rkey -Name "UninstallString"      `
        -Value "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$scriptPath`" -Uninstall"
    Set-ItemProperty -Path $rkey -Name "QuietUninstallString" `
        -Value "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$scriptPath`" -Uninstall -quiet"
}

Write-Host "`nCMake (ZIP) installation (v$versionString) complete. You may close this window when you’re ready."

Stop-Transcript

exit 0
