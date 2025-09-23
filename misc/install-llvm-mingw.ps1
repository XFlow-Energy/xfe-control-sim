<#
.SYNOPSIS
  Download and “portable‐style” install LLVM-MinGW (UCRT x86_64 ZIP) into a custom folder,
  then verify that clang.exe and friends are in place.  Supports a 100% silent,
  non‐MSI approach by defaulting to the GitHub “latest” release.

.PARAMETER installDir
  The directory into which to install LLVM-MinGW (e.g. "C:\llvm-mingw").

.PARAMETER llvmUrl
  (Optional) If you already know a direct download URL for a specific llvm-mingw ZIP,
  pass it here.  Otherwise the script queries GitHub’s API and picks the latest UCRT x86_64 ZIP.

.PARAMETER Uninstall
  If passed, attempt to remove llvm-mingw by deleting the folder, removing any PATH entries,
  and cleaning up the “portable” registry key.

.EXAMPLE
  # Install the latest automatically:
  .\install-llvm-mingw.ps1 -installDir "C:\llvm-mingw"

  # Uninstall:
  .\install-llvm-mingw.ps1 -Uninstall -installDir "C:\llvm-mingw"

  # Or explicitly install from a known URL:
  .\install-llvm-mingw.ps1 -installDir "C:\llvm-mingw" `
      -llvmUrl "https://github.com/mstorsjo/llvm-mingw/releases/download/20250601/llvm-mingw-20250601-ucrt-x86_64.zip"
#>
param(
    [switch]$Uninstall,
    [string]$llvmUrl     = "",                                               # If empty, we will fetch “latest” from GitHub
    [string]$installDir  = "C:\llvm-mingw",
    [string]$llvmZipPath = "$env:TEMP\llvm-mingw.zip"
)

# Start logging into the same folder the script lives in
$logPath = Join-Path $PSScriptRoot 'install.log'
Start-Transcript -Path $logPath -Append

# ──────────────────────────────────────────────────────────────────────────
# Derived paths
$binDir = Join-Path $installDir "bin"

# --------------------------------------------------------------------
# Helper: Remove a single folder from the machine-level PATH
function Remove-FromMachinePath($folderToRemove) {
    $envName  = "PATH"
    $existing = [Environment]::GetEnvironmentVariable($envName, [EnvironmentVariableTarget]::Machine)
    if (-not $existing) { return }

    # Split on ';' and keep only those segments that do NOT equal $folderToRemove
    $newParts = $existing -split ";" | Where-Object { $_ -and ($_ -ne $folderToRemove) }
    $newValue = $newParts -join ";"
    [Environment]::SetEnvironmentVariable($envName, $newValue, [EnvironmentVariableTarget]::Machine)
}

# --------------------------------------------------------------------
# If called with -Uninstall, immediately do the non‐interactive removal steps, then exit.
if ($Uninstall) {
    Write-Host "=== Uninstalling llvm-mingw (Portable) ==="

    # 1) Remove $binDir from MACHINE‐LEVEL PATH (whether or not it exists)
    Write-Host "Removing '$binDir' from Machine PATH (if present)…"
    Remove-FromMachinePath $binDir

    # 2) Delete the install directory, if it exists
    if (Test-Path $installDir) {
        Write-Host "`nRemoving directory: $installDir"
        try {
            Remove-Item -Recurse -Force $installDir -ErrorAction Stop
            Write-Host "Deleted $installDir."
        } catch {
            Write-Host "ERROR: Could not remove $installDir. Check for file locks and Administrator rights."
            exit 1
        }
    } else {
        Write-Host "No llvm-mingw installation found at $installDir."
    }

    # 3) Remove our own Uninstall registry key(s)
    $scriptPath    = $MyInvocation.MyCommand.Path
    $uninstallKeys = @(
        "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\LLVMMingwPortable",
        "HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\LLVMMingwPortable"
    )
    foreach ($key in $uninstallKeys) {
        if (Test-Path $key) {
            Write-Host "Deleting registry key: $key"
            try {
                Remove-Item -Path $key -Recurse -Force -ErrorAction Stop
                Write-Host "Deleted registry key: $key"
            } catch {
                Write-Host "WARNING: Could not delete registry key $key."
            }
        }
    }

    Write-Host "`nllvm-mingw uninstalled successfully."
    exit 0
}

# ──────────────────────────────────────────────────────────────────────────
# Otherwise: “Install” path (always non‐interactive once started)

# 1) Ensure we are running elevated (Administrator)
if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(`
    [Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Host "ERROR: You must run this script as Administrator."
    exit 1
}

# ── DYNAMIC LATEST RELEASE LOGIC ──
if (-not $llvmUrl) {
    Write-Host "Fetching latest llvm-mingw release information from GitHub..."

    try {
        # Query GitHub API for the `latest` release
        $release = Invoke-RestMethod -Uri "https://api.github.com/repos/mstorsjo/llvm-mingw/releases/latest" `
                                     -UseBasicParsing -ErrorAction Stop

        # Look for the asset whose name ends with "-ucrt-x86_64.zip"
        $asset = $release.assets | Where-Object { $_.name -match "-ucrt-x86_64\.zip$" } | Select-Object -First 1
        if (-not $asset) {
            Write-Host "ERROR: Could not find any asset ending in '-ucrt-x86_64.zip' in the latest release."
            exit 1
        }

        $llvmUrl = $asset.browser_download_url
        Write-Host "→ Latest llvm-mingw tag is '$($release.tag_name)', URL = $llvmUrl"
        $versionString = $release.tag_name.TrimStart("v")
    } catch {
        Write-Host "ERROR: Failed to fetch latest llvm-mingw release info from GitHub: $_"
        exit 1
    }
} else {
    # If user explicitly passed -llvmUrl, we still want to set a versionString placeholder
    Write-Host "Using user-supplied URL: $llvmUrl"
    # Derive a versionString from the URL if possible (e.g. .../20250514/... → "20250514"), else “custom”
    if ($llvmUrl -match "/([^/]+)-ucrt-x86_64\.zip$") {
        $versionString = $Matches[1]
    } else {
        $versionString = "custom"
    }
}

# 2) If $installDir already exists, remove it (upgrade path, no questions)
if (Test-Path $installDir) {
    Write-Host "Detected existing llvm-mingw at $installDir. Removing for upgrade..."
    try {
        Remove-Item -Recurse -Force $installDir -ErrorAction Stop
        Write-Host "Old installation removed."
    } catch {
        Write-Host "ERROR: Could not remove existing installation at $installDir. Check file locks or Admin rights."
        exit 1
    }

    # Also remove any leftover PATH entry
    if (Test-Path $binDir) {
        Remove-FromMachinePath $binDir
    }
}

# 3) Download the llvm-mingw ZIP
Write-Host "Downloading llvm-mingw from $llvmUrl …"
try {
    Invoke-WebRequest -Uri $llvmUrl -OutFile $llvmZipPath -UseBasicParsing -ErrorAction Stop
    Write-Host "Download complete: $llvmZipPath"
} catch {
    Write-Host "ERROR: Failed to download $llvmUrl"
    exit 1
}

# 4) Verify the ZIP exists
if (-not (Test-Path $llvmZipPath)) {
    Write-Host "ERROR: Downloaded file not found at $llvmZipPath"
    exit 1
}

# 5) Create $installDir (if needed) and extract the archive into it
Write-Host "Creating install directory: $installDir"
if (-not (Test-Path $installDir)) {
    New-Item -ItemType Directory -Path $installDir | Out-Null
}

Write-Host "Extracting llvm-mingw into $installDir …"
try {
    Expand-Archive -Path $llvmZipPath -DestinationPath $installDir -Force
    Write-Host "Extraction complete."
} catch {
    Write-Host "ERROR: Failed to expand archive: $_"
    exit 1
}

# 6) Flatten the extracted subfolder (if e.g. “llvm-mingw-20250514-ucrt-x86_64” was created)
$subfolder = Get-ChildItem -Path $installDir -Directory |
             Where-Object { $_.Name -match "^llvm-mingw-.*-ucrt-x86_64$" } |
             Select-Object -First 1
if ($subfolder) {
    Write-Host "Flattening extracted folder structure..."
    # Move everything under $installDir\<subfolder>\* into $installDir\
    Move-Item -Force -Path (Join-Path $installDir "$($subfolder.Name)\*") -Destination $installDir
    Remove-Item -Recurse -Force (Join-Path $installDir $subfolder.Name)
}

# 7) Verify that clang.exe now exists in $installDir\bin
if (-not (Test-Path (Join-Path $installDir "bin\clang.exe"))) {
    Write-Host "ERROR: clang.exe not found after extraction."
    Remove-Item -Path $llvmZipPath -Force -ErrorAction SilentlyContinue
    exit 1
}
Write-Host "llvm-mingw installed successfully to $installDir."

# 8) Clean up the downloaded ZIP
Write-Host "Cleaning up downloaded archive..."
Remove-Item -Path $llvmZipPath -Force -ErrorAction SilentlyContinue

# 9) Add $binDir to the MACHINE PATH (if not already present)
Write-Host "Updating machine PATH to include: $binDir"
$machinePath = [Environment]::GetEnvironmentVariable("PATH", [EnvironmentVariableTarget]::Machine)
if ($machinePath -notlike "*$binDir*") {
    [Environment]::SetEnvironmentVariable("PATH", "$machinePath;$binDir", [EnvironmentVariableTarget]::Machine)
    Write-Host "PATH updated. Please restart or open a new console to see changes."
} else {
    Write-Host "PATH already contains $binDir. Skipping."
}

# 10) Register the Uninstall key so Control Panel can call us with -Uninstall
Write-Host "Registering Uninstall key in the registry..."
$scriptPath     = $MyInvocation.MyCommand.Path
$uninstallPaths = @(
    "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\LLVMMingwPortable",
    "HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\LLVMMingwPortable"
)
foreach ($key in $uninstallPaths) {
    if (-not (Test-Path $key)) {
        New-Item -Path $key -Force | Out-Null
    }
    Set-ItemProperty -Path $key -Name "DisplayName"          -Value "llvm-mingw (Portable)"
    Set-ItemProperty -Path $key -Name "DisplayVersion"       -Value $versionString
    Set-ItemProperty -Path $key -Name "Publisher"            -Value "llvm-mingw Project"
    Set-ItemProperty -Path $key -Name "UninstallString"      `
        -Value "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$scriptPath`" -Uninstall"
    Set-ItemProperty -Path $key -Name "QuietUninstallString" `
        -Value "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$scriptPath`" -Uninstall -quiet"
}

Write-Host "llvm-mingw installation complete (version $versionString)."

Stop-Transcript

exit 0
