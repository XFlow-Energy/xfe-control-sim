# Define the w64devkit installer URL and local paths
$w64devkitUrl = "https://github.com/skeeto/w64devkit/releases/download/v2.0.0/w64devkit-x64-2.0.0.exe"
$w64devkitInstallerPath = "$env:TEMP\w64devkit-x64-2.0.0.exe"
$installDir = "C:\w64devkit"

# Check if w64devkit is already installed
if (Test-Path $installDir) {
    Write-Host "w64devkit is already installed at $installDir."
    $choice = Read-Host "Do you want to remove and reinstall w64devkit? (y/n)"
    if ($choice -eq "y") {
        Write-Host "Removing existing w64devkit installation..."
        Remove-Item -Recurse -Force $installDir
    } else {
        Write-Host "Installation cancelled."
        exit
    }
}

# Download the w64devkit installer
Write-Host "Downloading w64devkit..."
Invoke-WebRequest -Uri $w64devkitUrl -OutFile $w64devkitInstallerPath

# Verify the download
if (Test-Path $w64devkitInstallerPath) {
    Write-Host "Download complete. Launching the installer..."
} else {
    Write-Host "Failed to download w64devkit."
    exit
}

# Create the installation directory
if (-Not (Test-Path $installDir)) {
    New-Item -ItemType Directory -Path $installDir
}

# Launch the w64devkit installer
Write-Host "Installing w64devkit..."
Start-Process -FilePath $w64devkitInstallerPath -ArgumentList "/S", "/D=$installDir" -NoNewWindow -Wait

# Check if w64devkit was installed
if (Test-Path "$installDir\bin\w64devkit.exe") {
    Write-Host "w64devkit installed successfully at $installDir."
} else {
    Write-Host "w64devkit installation failed."
}

# Clean up the installer
Write-Host "Cleaning up installer..."
Remove-Item -Path $w64devkitInstallerPath -Force

# Optional: Add w64devkit\bin to the system PATH
Write-Host "Updating system PATH to include w64devkit\bin..."
$path = [System.Environment]::GetEnvironmentVariable("PATH", [System.EnvironmentVariableTarget]::Machine)
if ($path -notlike "*C:\w64devkit\bin*") {
    [System.Environment]::SetEnvironmentVariable("PATH", "$path;C:\w64devkit\bin", [System.EnvironmentVariableTarget]::Machine)
    Write-Host "System PATH updated. Please restart your system or open a new terminal session for the updated PATH to take effect."
} else {
    Write-Host "w64devkit\bin is already in the system PATH."
}