<#
.SYNOPSIS
  Download and “portable‐style” install Git for Windows into a custom folder,
  then verify that git.exe is in place.  Supports a 100% silent, non‐MSI approach.
  Also ensures GitHub CLI ("gh") is installed and on PATH.

.PARAMETER installDir
  The directory into which to install Git (e.g. "C:\Program Files\XFEControlSimDeps\Git").
  If this folder already exists, it will be removed first (upgrade path).

.PARAMETER Uninstall
  If passed, attempt to remove the portable Git folder, strip any PATH entries,
  delete the “GitPortable” registry key, and uninstall GitHub CLI.

.EXAMPLE
  # Install (or skip if already on PATH):
  .\install-git.ps1 -installDir "C:\Program Files\XFEControlSimDeps\Git"

  # Uninstall:
  .\install-git.ps1 -Uninstall -installDir "C:\Program Files\XFEControlSimDeps\Git"
#>

param(
    [switch] $Uninstall,
    [string] $installDir = "C:\Program Files\XFEControlSimDeps\Git"
)

# Start logging into the same folder the script lives in
$logPath = Join-Path $PSScriptRoot 'install.log'
Start-Transcript -Path $logPath -Append

# ----------------------------------------------------------------------------
function Remove-GitPortableRegistryKey {
	$registryPaths = @(
		"HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\GitPortable",
		"HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\GitPortable"
	)
	foreach ($keyPath in $registryPaths) {
		if (Test-Path $keyPath) {
			try {
				Remove-Item -Path $keyPath -Recurse -Force -ErrorAction Stop
				Write-Host "Deleted registry key: $keyPath"
			} catch {
				Write-Host "WARNING: Could not delete registry key $keyPath."
			}
		}
	}
}

function Ensure-GH {
	try {
		gh --version > $null 2>&1
		Write-Host "GitHub CLI found."
		return
	} catch {}
	Write-Host "GitHub CLI not found. Installing via winget..."
	if (Get-Command winget -ErrorAction SilentlyContinue) {
		winget install --id GitHub.cli -e --source winget --silent
		Write-Host "GitHub CLI installed."
	} else {
		Write-Host "Please install GitHub CLI from https://cli.github.com/"
	}
}

# ----------------------------------------------------------------------------
if ($Uninstall) {
	Write-Host "=== Uninstalling Git (Portable ZIP) ==="

	# 1) Verify elevation
	$isAdmin = (New-Object Security.Principal.WindowsPrincipal(
				   [Security.Principal.WindowsIdentity]::GetCurrent()
			   )).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
	if (-not $isAdmin) {
		Write-Host "ERROR: Must run as Administrator to uninstall."
		exit 1
	}

	# 2) Remove PATH entry that points to <installDir>\cmd (where git.exe lives)
	$gitCmdFolder = Join-Path $installDir "cmd"
	$oldPath = [Environment]::GetEnvironmentVariable("PATH",[EnvironmentVariableTarget]::Machine)
	if ($oldPath -like "*$gitCmdFolder*") {
		$newParts = $oldPath -split ";" |
					Where-Object { $_ -and ($_ -ne $gitCmdFolder) }
		[Environment]::SetEnvironmentVariable(
			"PATH",
			($newParts -join ";"),
			[EnvironmentVariableTarget]::Machine
		)
		Write-Host "Removed `$gitCmdFolder` from MACHINE PATH."
	}

	# 3) Delete the install directory
	if (Test-Path $installDir) {
		try {
			Remove-Item -Recurse -Force $installDir -ErrorAction Stop
			Write-Host "Deleted $installDir."
		} catch {
			Write-Host "ERROR: Could not remove $installDir. Check file locks or permissions."
			exit 1
		}
	} else {
		Write-Host "Directory not found: $installDir. Nothing to remove."
	}

	# 4) Remove our “GitPortable” registry key(s)
	Remove-GitPortableRegistryKey

	# 5) Uninstall GitHub CLI
	Write-Host "Uninstalling GitHub CLI..."
	if (Get-Command winget -ErrorAction SilentlyContinue) {
		winget uninstall --id GitHub.cli -e --source winget --silent
		Write-Host "GitHub CLI uninstalled."
	} else {
		Write-Host "winget not found; please uninstall GitHub CLI manually."
	}

	Write-Host "`nGit (Portable) has been uninstalled successfully."
	exit 0
}

# ----------------------------------------------------------------------------
# Otherwise: “Install” path

# 1) If git.exe is already on PATH, skip entirely
if (Get-Command git.exe -ErrorAction SilentlyContinue) {
	Write-Host "Git is already installed and found in PATH. No action needed."
} else {
	# 2) Ensure we are elevated (Administrator) for writing to Program Files, registry, and PATH
	$isAdmin = (New-Object Security.Principal.WindowsPrincipal(
				   [Security.Principal.WindowsIdentity]::GetCurrent()
			   )).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
	if (-not $isAdmin) {
		Write-Host "ERROR: This script must be run as Administrator."
		exit 1
	}

	# ── STEP A: Query GitHub’s API for the “latest” Git for Windows release ──
	Write-Host "Fetching latest Git for Windows release information from GitHub…"
	try {
		$release = Invoke-RestMethod `
			-Uri "https://api.github.com/repos/git-for-windows/git/releases/latest" `
			-UseBasicParsing -ErrorAction Stop

		# First try to find a “MinGit‐…‐64-bit.zip” asset (Recommended: small, pure ZIP).
		$mingitAsset = $release.assets |
			Where-Object { $_.name -match "^MinGit-.*-64-bit\.zip$" } |
			Select-Object -First 1

		if ($mingitAsset) {
			$downloadUrl   = $mingitAsset.browser_download_url
			$versionString = ($mingitAsset.name -replace "^MinGit-","") -replace "-64-bit\.zip$",""
			Write-Host "→ Found MinGit asset: $($mingitAsset.name)"
			Write-Host "   Version = $versionString"
		}
		else {
			# Fallback: see if a “PortableGit-…-64-bit.7z.exe” is present
			$portable7z = $release.assets |
				Where-Object { $_.name -match "^PortableGit-.*-64-bit\.7z\.exe$" } |
				Select-Object -First 1

			if ($portable7z) {
				$downloadUrl   = $portable7z.browser_download_url
				$versionString = ($portable7z.name -replace "^PortableGit-","") -replace "-64-bit\.7z\.exe$",""
				Write-Host "→ Found PortableGit .7z.exe asset: $($portable7z.name)"
				Write-Host "   Version = $versionString"
			}
			else {
				throw "Could not find either a MinGit-*-64-bit.zip or a PortableGit-*-64-bit.7z.exe in the latest release."
			}
		}
	}
	catch {
		Write-Host "ERROR: Failed to fetch or parse GitHub release info: $_"
		exit 1
	}

	# ── STEP B: Download and extract into $installDir ──
	if (Test-Path $installDir) {
		Write-Host "Removing existing directory: $installDir"
		try {
			Remove-Item -Recurse -Force $installDir -ErrorAction Stop
			Write-Host "Deleted old folder."
		} catch {
			Write-Host "ERROR: Could not remove $installDir. Check file locks or permissions."
			exit 1
		}
	}
	New-Item -ItemType Directory -Path $installDir | Out-Null

	$tempZip = Join-Path $env:TEMP "GitPortable.zip"
	Write-Host "Downloading $downloadUrl …"
	try {
		Invoke-WebRequest -Uri $downloadUrl -OutFile $tempZip -UseBasicParsing -ErrorAction Stop
		Write-Host "Download complete: $tempZip"
	} catch {
		Write-Host "ERROR: Failed to download $downloadUrl"
		exit 1
	}

	if ($downloadUrl -match "\.zip$") {
		Write-Host "Extracting ZIP into $installDir …"
		try {
			Expand-Archive -Path $tempZip -DestinationPath $installDir -Force
			Write-Host "Extraction complete."
		} catch {
			Write-Host "ERROR: Failed to extract ZIP: $_"
			exit 1
		}
	}
	else {
		Write-Host "Running self‐extracting 7z installer into $installDir …"
		try {
			& $tempZip /SILENT /DIR="$installDir" 2>&1 | Write-Host
			Write-Host "PortableGit extracted."
		} catch {
			Write-Host "ERROR: Failed to run PortableGit .7z.exe: $_"
			exit 1
		}
	}

	try { Remove-Item -Path $tempZip -Force -ErrorAction SilentlyContinue } catch {}

	# ── STEP C: Add `<installDir>\cmd` to MACHINE PATH ──
	$gitBinDir = Join-Path $installDir "cmd"
	if (-not (Test-Path (Join-Path $gitBinDir "git.exe"))) {
		Write-Host "ERROR: git.exe was not found under $gitBinDir. Extraction may have failed."
		exit 1
	}
	$machinePath = [Environment]::GetEnvironmentVariable("PATH",[EnvironmentVariableTarget]::Machine)
	if ($machinePath -notlike "*$gitBinDir*") {
		[Environment]::SetEnvironmentVariable(
			"PATH",
			"$machinePath;$gitBinDir",
			[EnvironmentVariableTarget]::Machine
		)
		Write-Host "Added `$gitBinDir` to MACHINE PATH. You may need to open a new console to see it."
	} else {
		Write-Host "`$gitBinDir` was already in MACHINE PATH."
	}

	# ── STEP D: Create “GitPortable” Uninstall registry key ──
	Write-Host "Registering “GitPortable” Uninstall key…"
	$scriptPath    = $MyInvocation.MyCommand.Path
	$uPaths = @(
		"HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\GitPortable",
		"HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\GitPortable"
	)
	foreach ($u in $uPaths) {
		if (-not (Test-Path $u)) {
			New-Item -Path $u -Force | Out-Null
		}
		Set-ItemProperty -Path $u -Name "DisplayName"          -Value "Git for Windows (Portable)"
		Set-ItemProperty -Path $u -Name "DisplayVersion"       -Value $versionString
		Set-ItemProperty -Path $u -Name "Publisher"            -Value "Git for Windows Project"
		Set-ItemProperty -Path $u -Name "UninstallString"      `
			-Value "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$scriptPath`" -Uninstall"
		Set-ItemProperty -Path $u -Name "QuietUninstallString" `
			-Value "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$scriptPath`" -Uninstall -Quiet"
	}

	# ── STEP E: Ensure GitHub CLI is available ──
	Write-Host "Ensuring GitHub CLI is installed and on PATH…"
	Ensure-GH

	Write-Host ""
	Write-Host "Git for Windows (Portable) v$versionString has been installed to $installDir."
}

Stop-Transcript

exit 0
