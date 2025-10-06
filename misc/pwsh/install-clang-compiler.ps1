param(
    [switch]$Uninstall,
    [string]$installDir   = "C:\Program Files\LLVM",
    [string]$llvmUrl      = "https://github.com/llvm/llvm-project/releases/download/llvmorg-18.1.8/LLVM-18.1.8-win64.exe",
    [string]$installerExe = "$env:TEMP\LLVM-18.1.8-win64.exe"
)

# Start logging into the same folder the script lives in
$logPath = Join-Path $PSScriptRoot 'install.log'
Start-Transcript -Path $logPath -Append

# Derived paths
$binDir    = Join-Path $installDir "bin"
$clangPath = Join-Path $binDir "clang.exe"

function Remove-FromMachinePath($folderToRemove) {
    $envName  = "PATH"
    $existing = [Environment]::GetEnvironmentVariable($envName, [EnvironmentVariableTarget]::Machine)
    if (-not $existing) { return }
    $newParts = $existing -split ";" | Where-Object { $_ -and ($_ -ne $folderToRemove) }
    $newValue = $newParts -join ";"
    [Environment]::SetEnvironmentVariable($envName, $newValue, [EnvironmentVariableTarget]::Machine)
}

# -------------------------
# Uninstall logic
if ($Uninstall) {
    Write-Host "=== Uninstalling LLVM/Clang ==="

    # 1) Ensure elevation
    if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(`
        [Security.Principal.WindowsBuiltInRole] "Administrator")) {
        Write-Host "ERROR: Must run as Administrator to uninstall."
        exit 1
    }

    # 2) Attempt MSI uninstall via registry lookup
    Write-Host "Looking for LLVM uninstall registry entry..."
    $key = Get-ChildItem 'HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall' `
           | Get-ItemProperty `
           | Where-Object { $_.DisplayName -like 'LLVM*' }
    if ($key) {
        Write-Host "Found LLVM: $($key.DisplayName). Uninstalling via msiexec..."
        try {
            Start-Process msiexec.exe -ArgumentList '/x', $key.PSChildName, '/quiet', '/norestart' -Wait -NoNewWindow
        } catch {
            Write-Host "WARNING: msiexec uninstall via registry failed. Will remove folder manually."
        }
    } else {
        Write-Host "No MSI entry found. Will remove folder manually if present."
    }

    # 3) Remove any remaining installation directory
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
        Write-Host "LLVM directory not found at $installDir."
    }

    # 4) Remove from Machine PATH
    if (Test-Path $binDir) {
        Write-Host "Removing '$binDir' from machine PATH..."
        Remove-FromMachinePath $binDir
    }

    # 5) Remove custom Uninstall registry keys (32-bit & 64-bit)
    $scriptPath    = $MyInvocation.MyCommand.Path
    $uninstallKeys = @(
        "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\LLVMPortable",
        "HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\LLVMPortable"
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

    Write-Host "LLVM/Clang uninstalled successfully."
    exit 0
}

# -------------------------
# Installation logic

# 1) Ensure elevation
if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(`
    [Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Host "ERROR: Must run as Administrator."
    exit 1
}

# 2) If Clang already exists, prompt to remove and reinstall
if (Test-Path $clangPath) {
    Write-Host "Clang is already installed at $clangPath."
    $choice = Read-Host "Do you want to remove and reinstall Clang? (y/n)"
    if ($choice -eq "y") {
        Write-Host "Uninstalling existing Clang..."
        $key = Get-ChildItem 'HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall' `
               | Get-ItemProperty `
               | Where-Object { $_.DisplayName -like 'LLVM*' }
        if ($key) {
            try {
                Start-Process msiexec.exe -ArgumentList '/x', $key.PSChildName, '/quiet', '/norestart' -Wait -NoNewWindow
            } catch {
                Write-Host "WARNING: msiexec uninstall via registry failed. Removing folder manually..."
                if (Test-Path $installDir) {
                    try {
                        Remove-Item -Recurse -Force $installDir -ErrorAction Stop
                    } catch {
                        Write-Host "ERROR: Could not remove $installDir. Check permissions."
                        exit 1
                    }
                }
            }
        } else {
            Write-Host "No MSI entry found. Removing folder manually..."
            if (Test-Path $installDir) {
                try {
                    Remove-Item -Recurse -Force $installDir -ErrorAction Stop
                } catch {
                    Write-Host "ERROR: Could not remove $installDir. Check permissions."
                    exit 1
                }
            }
        }
        # Remove from PATH as well
        if (Test-Path $binDir) {
            Remove-FromMachinePath $binDir
        }
    } else {
        Write-Host "Installation cancelled."
        exit 0
    }
}

# 3) Download the LLVM installer
Write-Host "Downloading LLVM from $llvmUrl ..."
try {
    Invoke-WebRequest -Uri $llvmUrl -OutFile $installerExe -UseBasicParsing -ErrorAction Stop
} catch {
    Write-Host "ERROR: Failed to download LLVM installer."
    exit 1
}

# 4) Verify the download
if (Test-Path $installerExe) {
    Write-Host "Download complete. Launching the installer..."
} else {
    Write-Host "ERROR: Installer not found at $installerExe."
    exit 1
}

# 5) Launch the LLVM installer silently, specifying the install directory
Write-Host "Installing LLVM to $installDir ..."
try {
    Start-Process -FilePath $installerExe -ArgumentList "/S","/D=$installDir" -NoNewWindow -Wait -ErrorAction Stop
} catch {
    Write-Host "ERROR: Installation process failed."
    Remove-Item -Path $installerExe -Force -ErrorAction SilentlyContinue
    exit 1
}

# 6) Check if Clang was installed
if (Test-Path $clangPath) {
    Write-Host "Clang installed successfully at $clangPath."
} else {
    Write-Host "ERROR: Clang installation failed."
    Remove-Item -Path $installerExe -Force -ErrorAction SilentlyContinue
    exit 1
}

# 7) Clean up the installer
Write-Host "Cleaning up installer..."
try {
    Remove-Item -Path $installerExe -Force -ErrorAction Stop
} catch {
    Write-Host "WARNING: Could not delete installer at $installerExe."
}

# 8) Update Machine PATH to include LLVM\bin
Write-Host "Updating system PATH to include '$binDir' ..."
$path = [Environment]::GetEnvironmentVariable("PATH", [EnvironmentVariableTarget]::Machine)
if ($path -notlike "*$binDir*") {
    [Environment]::SetEnvironmentVariable("PATH", "$path;$binDir", [EnvironmentVariableTarget]::Machine)
    Write-Host "System PATH updated. Restart or open a new session to see changes."
} else {
    Write-Host "LLVM\bin already in system PATH."
}

# 9) Create Uninstall registry key (32-bit & 64-bit)
Write-Host "Creating Uninstall registry key..."
$scriptPath    = $MyInvocation.MyCommand.Path
$uninstallKeys = @(
    "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\LLVMPortable",
    "HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\LLVMPortable"
)
foreach ($keyPath in $uninstallKeys) {
    if (-not (Test-Path $keyPath)) {
        New-Item -Path $keyPath -Force | Out-Null
    }
    Set-ItemProperty -Path $keyPath -Name "DisplayName"          -Value "LLVM/Clang (Portable)"
    Set-ItemProperty -Path $keyPath -Name "DisplayVersion"       -Value "18.1.8"
    Set-ItemProperty -Path $keyPath -Name "Publisher"            -Value "LLVM Project"
    Set-ItemProperty -Path $keyPath -Name "UninstallString"      `
        -Value "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$scriptPath`" -Uninstall"
    Set-ItemProperty -Path $keyPath -Name "QuietUninstallString" `
        -Value "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$scriptPath`" -Uninstall -quiet"
}

Write-Host "LLVM/Clang installation complete."

Stop-Transcript

exit 0
