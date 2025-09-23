param(
    [switch]$Uninstall,
    [string]$installDir  = "C:\PROGRA~1\Ninja",
    [string]$ninjaDir    = "C:\ninja",
    [string]$downloadUrl = ""   # we’ll fill this dynamically if left blank
)

# Start logging into the same folder the script lives in
$logPath = Join-Path $PSScriptRoot 'install.log'
Start-Transcript -Path $logPath -Append

# --------------------------------------------------------------------
# Internal helper: remove any machine-wide PATH entries that match $folderToRemove
function Remove-FromMachinePath($folderToRemove) {
    $envName  = "PATH"
    $existing = [Environment]::GetEnvironmentVariable($envName, [EnvironmentVariableTarget]::Machine)
    if (-not $existing) { return }

    $newParts = $existing -split ";" | Where-Object {
        $_ -and ($_ -ne $folderToRemove) -and (-not ($_ -like "$folderToRemove*"))
    }
    $newValue = $newParts -join ";"
    [Environment]::SetEnvironmentVariable($envName, $newValue, [EnvironmentVariableTarget]::Machine)
}

# --------------------------------------------------------------------
# If called with -Uninstall, immediately perform the “uninstall” path, then exit.
if ($Uninstall) {
    Write-Host "=== Uninstalling Ninja ==="

    # 1) Remove the install directory (if it still exists)
    if (Test-Path $installDir) {
        Write-Host "Removing directory: $installDir"
        try {
            Remove-Item -Recurse -Force $installDir -ErrorAction Stop
            Write-Host "Deleted $installDir."
        } catch {
            Write-Host "ERROR: Could not delete $installDir. Ensure no files are locked and you have Administrator rights."
            exit 1
        }
    } else {
        Write-Host "No Ninja installation found at $installDir."
    }

    # 2) Remove the temporary extraction directory, if it still exists
    if (Test-Path $ninjaDir) {
        Write-Host "Removing temporary folder: $ninjaDir"
        try {
            Remove-Item -Recurse -Force $ninjaDir -ErrorAction Stop
            Write-Host "Deleted $ninjaDir."
        } catch {
            Write-Host "WARNING: Could not delete $ninjaDir. Please remove it manually."
        }
    }

    # 3) Strip any leftover PATH entry pointing at $installDir
    Write-Host "Cleaning PATH: removing entries that start with '$installDir'..."
    Remove-FromMachinePath $installDir

    # 4) Delete our own “NinjaPortable” Uninstall registry keys (both 32-bit & 64-bit locations)
    $uninstallKeys = @(
        "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\NinjaPortable",
        "HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\NinjaPortable"
    )
    foreach ($rk in $uninstallKeys) {
        if (Test-Path $rk) {
            Write-Host "Deleting registry key: $rk"
            Remove-Item -Path $rk -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    Write-Host "Ninja has been uninstalled successfully."
    exit 0
}

# ────────────────────────────────────────────────────────────────────
# Otherwise: “install” path (always non-interactive)

# (1) Make sure we are running as Administrator
if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(`
    [Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Host "ERROR: This installer must be run as Administrator."
    exit 1
}

# Derived paths
$zipFilePath  = Join-Path $ninjaDir "ninja.zip"
$ninjaBinary  = Join-Path $installDir "ninja.exe"

# (2) If Ninja is already present, remove it (no user prompt)
if (Test-Path $ninjaBinary) {
    Write-Host "Detected existing Ninja at $installDir. Removing for upgrade…"
    try {
        Remove-Item -Recurse -Force $installDir -ErrorAction Stop
        Write-Host "Old Ninja installation removed."
    } catch {
        Write-Host "ERROR: Could not delete existing Ninja at $installDir. Ensure no files are in use and you have Admin rights."
        exit 1
    }
    # Also strip any leftover PATH entries for $installDir
    Remove-FromMachinePath $installDir
}

# (3) Create a fresh temporary folder $ninjaDir
if (-not (Test-Path $ninjaDir)) {
    Write-Host "Creating temporary folder: $ninjaDir"
    New-Item -ItemType Directory -Path $ninjaDir | Out-Null
}

# ── DYNAMIC LATEST RELEASE LOGIC ──
if (-not $downloadUrl) {
    Write-Host "Fetching latest Ninja release information from GitHub..."
    try {
        # Query GitHub API for the “latest” release
        $release = Invoke-RestMethod -Uri "https://api.github.com/repos/ninja-build/ninja/releases/latest" `
                                     -UseBasicParsing -ErrorAction Stop

        # Find the asset whose name looks like “ninja-win.zip”
        $asset = $release.assets | Where-Object { $_.name -match "ninja-win\.zip" } | Select-Object -First 1
        if (-not $asset) {
            Write-Host "ERROR: Could not find any asset named 'ninja-win.zip' in the latest release."
            exit 1
        }

        $downloadUrl = $asset.browser_download_url
        Write-Host "→ Latest version is '$($release.tag_name)', URL = $downloadUrl"
    } catch {
        Write-Host "ERROR: Failed to fetch latest release info from GitHub: $_"
        exit 1
    }
} else {
    Write-Host "Using user-supplied download URL: $downloadUrl"
}

# (4) Download the Ninja ZIP from GitHub
Write-Host "Downloading Ninja from $downloadUrl …"
try {
    Invoke-WebRequest -Uri $downloadUrl -OutFile $zipFilePath -UseBasicParsing -ErrorAction Stop
    Write-Host "Download succeeded: $zipFilePath"
} catch {
    Write-Host "ERROR: Failed to download Ninja. Check internet connection or the URL."
    exit 1
}

# (5) Verify the ZIP actually exists
if (-not (Test-Path $zipFilePath)) {
    Write-Host "ERROR: ZIP file not found at $zipFilePath. Exiting."
    exit 1
}

# (6) Extract the ZIP to $ninjaDir
Write-Host "Extracting Ninja into $ninjaDir …"
try {
    Expand-Archive -Path $zipFilePath -DestinationPath $ninjaDir -Force
    Write-Host "Extraction completed."
} catch {
    Write-Host "ERROR: Failed to expand archive: $_"
    exit 1
}

# (7) Find ninja.exe somewhere under $ninjaDir
$ninjaExeFound = Get-ChildItem -Path $ninjaDir -Recurse -Filter ninja.exe | Select-Object -First 1
if (-not $ninjaExeFound) {
    Write-Host "ERROR: Could not locate ninja.exe inside $ninjaDir after extraction."
    exit 1
}

# (8) Create the final install directory under Program Files
if (-not (Test-Path $installDir)) {
    Write-Host "Creating install folder: $installDir"
    New-Item -ItemType Directory -Path $installDir | Out-Null
}

# (9) Move ninja.exe into $installDir
Write-Host "Moving ninja.exe to $installDir …"
try {
    Move-Item -Force -Path $ninjaExeFound.FullName -Destination $ninjaBinary
    Write-Host "ninja.exe installed to $installDir."
} catch {
    Write-Host "ERROR: Could not move ninja.exe into $installDir. Check permissions or file locks."
    exit 1
}

# (10) Clean up the temporary extraction directory
Write-Host "Cleaning up temporary files…"
try {
    Remove-Item -Recurse -Force $ninjaDir -ErrorAction Stop
    Write-Host "Temporary folder $ninjaDir removed."
} catch {
    Write-Host "WARNING: Could not remove $ninjaDir. Please delete it manually."
}

# (11) Add $installDir to the machine-wide PATH (if not already present)
Write-Host "Updating machine PATH to include $installDir …"
$machinePath = [Environment]::GetEnvironmentVariable("PATH", [EnvironmentVariableTarget]::Machine)
if ($machinePath -notlike "*$installDir*") {
    [Environment]::SetEnvironmentVariable("PATH", "$machinePath;$installDir", [EnvironmentVariableTarget]::Machine)
    Write-Host "PATH updated. You may need to open a fresh terminal or reboot to see changes."
} else {
    Write-Host "PATH already contains $installDir. Skipping."
}

# (12) Register our own “NinjaPortable” Uninstall key (both 32-bit & 64-bit locations)
Write-Host "Creating Uninstall registry key for Ninja (Portable)…"
$scriptPath     = $MyInvocation.MyCommand.Path
$uninstallKeys  = @(
    "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\NinjaPortable",
    "HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\NinjaPortable"
)

# Derive version string from the tag name (optional)
# e.g. "v1.12.1" → "1.12.1"
$versionString = ""
if ($release -and $release.tag_name) {
    $versionString = $release.tag_name.TrimStart("v")
} else {
    # fallback if parsing fails
    $versionString = "latest"
}

foreach ($keyPath in $uninstallKeys) {
    if (-not (Test-Path $keyPath)) {
        New-Item -Path $keyPath -Force | Out-Null
    }
    Set-ItemProperty -Path $keyPath -Name "DisplayName"          -Value "Ninja (Portable)"
    Set-ItemProperty -Path $keyPath -Name "DisplayVersion"       -Value $versionString
    Set-ItemProperty -Path $keyPath -Name "Publisher"            -Value "Ninja Build"
    Set-ItemProperty -Path $keyPath -Name "UninstallString"      `
        -Value "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$scriptPath`" -Uninstall"
    Set-ItemProperty -Path $keyPath -Name "QuietUninstallString" `
        -Value "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$scriptPath`" -Uninstall -quiet"
}

Write-Host "Ninja installation complete (version $versionString)."

Stop-Transcript

exit 0
