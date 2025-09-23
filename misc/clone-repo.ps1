<#
.SYNOPSIS
  Clone one or more GitHub repositories after authenticating with GitHub CLI,
  prompting for selection if none are specified.

.PARAMETER Destination
  Directory to clone the repositories into.

.PARAMETER Repo
  One or more GitHub repositories in 'owner/name' format.

.PARAMETER Select
  If passed (or if no Repo is given), prompts you to choose from
  the repositories in XFlow-Energy, XFlow-Controller, or your own user account.

.EXAMPLE
  # Clone two repos by name:
  .\clone-repo.ps1 -Destination "C:\repos" -Repo "XFlow-Energy/XFLOW-CONTROL-SIM","XFlow-Jason/iobb"

  # Or interactively select multiple:
  .\clone-repo.ps1 -Destination "C:\repos" -Select
#>

param(
	[string] $Destination,
	[string[]] $Repo   = @(),
	[switch] $Select
)

# Start logging into the same folder the script lives in
$logPath = Join-Path $PSScriptRoot 'install.log'
Start-Transcript -Path $logPath -Append

$ErrorActionPreference = 'Stop'

# Refresh this session’s PATH from the machine‐level PATH
$machinePath = [Environment]::GetEnvironmentVariable('PATH','Machine')
$processPath = [Environment]::GetEnvironmentVariable('PATH','Process')
$env:PATH    = "$machinePath;$processPath"

function Ensure-GH {
	try { gh --version > $null 2>&1; Write-Host "GitHub CLI found."; return } catch {}
	Write-Host "GitHub CLI not found. Installing via winget..."
	if (Get-Command winget -ErrorAction SilentlyContinue) {
		winget install --id GitHub.cli -e --source winget --silent
		# reload PATH
		$machinePath = [Environment]::GetEnvironmentVariable('PATH','Machine')
		$processPath = [Environment]::GetEnvironmentVariable('PATH','Process')
		$env:PATH    = "$machinePath;$processPath"
		try { gh --version > $null 2>&1; Write-Host "GitHub CLI is now installed."; return } catch { Write-Host "ERROR: gh.exe still not found after install."; exit 1 }
	} else { Write-Host "Please install GitHub CLI from https://cli.github.com/"; exit 1 }
}

Ensure-GH

Write-Host "Authenticating with GitHub (web flow)…"
gh auth login --hostname github.com --git-protocol https --web
Write-Host "Configuring Git to use GitHub credentials…"
gh auth setup-git --hostname github.com --force

# if none specified, prompt
if ($Select -or -not $Repo) { $Select = $true }

if ($Select) {
	Write-Host "Fetching repositories you can access in XFlow-Energy, XFlow-Controller, and your account…"
	try { $repos1 = (gh repo list XFlow-Energy     --limit 200 --json nameWithOwner | ConvertFrom-Json).nameWithOwner } catch { $repos1 = @() }
	try { $repos2 = (gh repo list XFlow-Controller --limit 200 --json nameWithOwner | ConvertFrom-Json).nameWithOwner } catch { $repos2 = @() }
	try {
		$user = gh api user --jq .login
		$repos3 = (gh repo list $user --limit 200 --json nameWithOwner | ConvertFrom-Json).nameWithOwner
	} catch { $repos3 = @() }
	$allRepos = $repos1 + $repos2 + $repos3
	if (-not $allRepos) { Write-Host "No accessible repos found."; exit 1 }
	for ($i=0; $i -lt $allRepos.Count; $i++) { Write-Host "[$($i+1)] $($allRepos[$i])" }
	do { $choice = Read-Host "Enter numbers (comma-separated, e.g. 1,3) of repos to clone" } until ($choice -match '^[0-9, ]+$')
	$indices = $choice -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ } | ForEach-Object { [int]$_ - 1 }
	$Repo = $indices | Where-Object { $_ -ge 0 -and $_ -lt $allRepos.Count } | ForEach-Object { $allRepos[$_] }
	Write-Host "Selected: $($Repo -join ', ')"
}

if (-not (Test-Path $Destination)) { New-Item -ItemType Directory -Path $Destination | Out-Null }

foreach ($r in $Repo) {
	$repoName   = Split-Path $r -Leaf
	$basePath   = Join-Path $Destination $repoName
	$clonePath  = $basePath
	$count      = 1
	while (Test-Path $clonePath) {
		$clonePath = "$basePath`_$count"
		$count++
	}
	Write-Host "Cloning $r into $clonePath"
	New-Item -ItemType Directory -Path $clonePath | Out-Null
	gh repo clone $r $clonePath
}

Write-Host "All clones complete."

Stop-Transcript
