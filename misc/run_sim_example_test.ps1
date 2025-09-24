# misc\launch_sim_example_test.ps1
param(
	[int]$REBUILD = 0
)

$ErrorActionPreference = "Stop"

# Resolve repo root via git
$XFE_CONTROL_SIM_ROOT = (& git rev-parse --show-toplevel).Trim()
if (-not $XFE_CONTROL_SIM_ROOT) {
	Write-Error "Failed to determine repo root. Is Git installed and are you inside the repo?"
	exit 2
}

$SIM_EXAMPLE = Join-Path $XFE_CONTROL_SIM_ROOT "sim_example"
$TMP_ROOT = Join-Path (Split-Path -Parent $XFE_CONTROL_SIM_ROOT) "sim_example_test"

Write-Host "→ Testing sim_example in temporary dir: $TMP_ROOT"

# 1) recreate or skip
if ($REBUILD -eq 1)
{
	if (Test-Path $TMP_ROOT)
	{
		Remove-Item -Recurse -Force $TMP_ROOT
	}
}

if (-not (Test-Path $TMP_ROOT))
{
	Write-Host "→ Copying sim_example to $TMP_ROOT"
	Copy-Item -Recurse -Force $SIM_EXAMPLE $TMP_ROOT
}

# 2) Build + run via the launcher in the copied folder
Write-Host "→ Building + running via sim_example/misc/run_xfe_control_sim.ps1 (RECOMPILE_OR_NOT=$REBUILD)"
Push-Location (Join-Path $TMP_ROOT "misc")
& .\run_xfe_control_sim.ps1 $REBUILD
$LAUNCH_EXIT = $LASTEXITCODE
Pop-Location

# 3) outcome
if ($LAUNCH_EXIT -ne 0)
{
	Write-Error "❌ sim_example run failed (exit $LAUNCH_EXIT)"
}
else
{
	Write-Host "✅ sim_example run succeeded"
}

# 4) validate log file contents before cleanup
$LOG_FILE = Join-Path $TMP_ROOT "log\log_data\xfe-control-sim-simulation-output.log"
Write-Host "→ Validating log file: $LOG_FILE"

$LOG_OK = $true
if (-not (Test-Path $LOG_FILE))
{
	Write-Error "❌ Log file not found."
	$LOG_OK = $false
}
else
{
	$hasProgram = Select-String -Path $LOG_FILE -SimpleMatch "Program Duration:" -Quiet
	if (-not $hasProgram)
	{
		Write-Error "❌ Missing 'Program Duration:' line."
		$LOG_OK = $false
	}

	$hasWrite = Select-String -Path $LOG_FILE -SimpleMatch "write Duration:" -Quiet
	if (-not $hasWrite)
	{
		Write-Error "❌ Missing 'write Duration:' line."
		$LOG_OK = $false
	}

	$lastNonEmpty = Get-Content -LiteralPath $LOG_FILE | Where-Object { $_.Trim().Length -gt 0 } | Select-Object -Last 1
	if ($null -eq $lastNonEmpty -or ($lastNonEmpty -notlike "*Closing Program*"))
	{
		Write-Error "❌ Last non-empty line is not 'Closing Program'."
		if ($null -ne $lastNonEmpty) { Write-Error ("   Last line was: " + $lastNonEmpty) }
		$LOG_OK = $false
	}

	$errLines = Select-String -Path $LOG_FILE -SimpleMatch "ERROR"
	if ($errLines)
	{
        Write-Error "❌ Found error lines in log:"
		$errLines | ForEach-Object { Write-Error $_.Line }
		$LOG_OK = $false
	}
}

# 5) cleanup behavior depends on validation
Push-Location $XFE_CONTROL_SIM_ROOT
if ($LOG_OK)
{
	Write-Host "✅ Log validation passed. Cleaning up temp folder: $TMP_ROOT"
	Remove-Item -Recurse -Force $TMP_ROOT
	Pop-Location
	exit $LAUNCH_EXIT
}
else
{
	Write-Error "⚠️  Log validation failed. Preserving temp folder for inspection: $TMP_ROOT"
	Write-Host ("   You can inspect the log with: `n`  Get-Content -Path `"" + $LOG_FILE + "`" -Raw") -NoNewline
	Pop-Location
	if ($LAUNCH_EXIT -ne 0) { exit $LAUNCH_EXIT } else { exit 1 }
}