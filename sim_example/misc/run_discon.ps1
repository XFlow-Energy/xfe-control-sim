# sim_example\misc\launch_discon.ps1
param(
	[int]$RECOMPILE_OR_NOT = 0
)

# Script & project dirs
$SCRIPT_DIR = $PSScriptRoot
$SIM_EXAMPLE_DIR = (Resolve-Path (Join-Path $SCRIPT_DIR "..")).Path
$BUILD_DIR = Join-Path $SIM_EXAMPLE_DIR "build"

# Helpers
function Find-Tool {
	param([string]$Name,[string[]]$CandidatePaths)
	foreach ($p in $CandidatePaths) { if (Test-Path $p) { return $p } }
	$cmd = Get-Command $Name -ErrorAction SilentlyContinue
	if ($cmd) { return $cmd.Path }
	return $null
}

# Optional PATHVARS (match your Bash toggles)
$PATHVARS = @(
	# "-DRUN_CPPCHECK=OFF"
	# "-DRUN_IWYU=OFF"
	"-DRUN_CLANG_TIDY=ON"
	# "-DRUN_SCAN_BUILD=OFF"
	# "-DRUN_FLAWFINDER=OFF"
)

if ($RECOMPILE_OR_NOT -eq 1) {
	if (Test-Path $BUILD_DIR) {
		Remove-Item -Recurse -Force $BUILD_DIR
	}
	$cppcheckCache = Join-Path $env:USERPROFILE ".cache\cppcheck"
	if (Test-Path $cppcheckCache) {
		Remove-Item -Recurse -Force $cppcheckCache -ErrorAction SilentlyContinue
	}
	New-Item -ItemType Directory -Force -Path $BUILD_DIR | Out-Null

	# Verbose & parallelism
	$VERBOSE = 1
	$NPROC = [int]($env:NUMBER_OF_PROCESSORS); if ($NPROC -lt 1) { $NPROC = 1 }

	# Ninja vs fallback (cmake --build)
	$NinjaPath = Find-Tool -Name "ninja.exe" -CandidatePaths @(
		"$env:ProgramFiles\Ninja\ninja.exe",
		"$env:ProgramFiles(x86)\Ninja\ninja.exe",
		"C:\ninja\ninja.exe"
	)
	$UseNinja = $null -ne $NinjaPath
	if ($UseNinja) {
# Generator and build command
		$GENERATOR = "-G Ninja"
		if ($VERBOSE -eq 1) {
			$BUILD_CMD = @($NinjaPath, "-v", "-j", "$NPROC")
		} else {
			$BUILD_CMD = @($NinjaPath, "-j", "$NPROC")
		}
	} else {
		$GENERATOR = ""
		$BUILD_CMD = @("cmake", "--build", "$BUILD_DIR", "--config", "Release", "--parallel", "$NPROC")
	}

	# OS / CI
	$InCI = ($env:GITHUB_ACTIONS -eq "true")
	if ($InCI) { $OS = $env:RUNNER_OS } else { $OS = "Windows" }

	# Compilers (prefer LLVM; fall back to llvm-mingw; finally PATH)
	if ($InCI) {
		switch ($OS) {
			"Windows" {
				$CC = "C:/deps/llvm-mingw/bin/clang.exe"
				$CXX = "C:/deps/llvm-mingw/bin/clang++.exe"
			}
			default { throw "Unsupported OS in PowerShell script: $OS" }
		}
	} else {
		$CC = Find-Tool -Name "clang.exe" -CandidatePaths @(
			"$env:ProgramFiles\LLVM\bin\clang.exe",
			"$env:ProgramFiles(x86)\LLVM\bin\clang.exe",
			"C:\deps\llvm-mingw\bin\clang.exe"
		)
		$CXX = Find-Tool -Name "clang++.exe" -CandidatePaths @(
			"$env:ProgramFiles\LLVM\bin\clang++.exe",
			"$env:ProgramFiles(x86)\LLVM\bin\clang++.exe",
			"C:\deps\llvm-mingw\bin\clang++.exe"
		)
		if (-not $CC -or -not $CXX) { throw "Could not find clang/clang++. Install LLVM (C:\Program Files\LLVM) or add to PATH." }
	}

	# Verbose makefile flag
	if ($VERBOSE -eq 1) { $CMAKE_VERBOSE_FLAG = "ON" } else { $CMAKE_VERBOSE_FLAG = "OFF" }

	# Deps prefix (CI Windows parity)
	$CMAKE_PREFIX_PATH = ""
	if ($InCI -and $OS -eq "Windows") {
		$CMAKE_PREFIX_PATH = "C:/deps/gsl-install;C:/deps/jansson-install;C:/deps/libmodbus"
	}

	# Build type (forced Release)
	$BUILD_TYPE = "Release"

	# Configure
	$cmakeArgs = @()
	if ($GENERATOR) { $cmakeArgs += $GENERATOR }
	$cmakeArgs += @(
		"-B","$BUILD_DIR",
		"-S","$SIM_EXAMPLE_DIR",
		"-DCMAKE_BUILD_TYPE=$BUILD_TYPE",
		"-DCMAKE_VERBOSE_MAKEFILE=$CMAKE_VERBOSE_FLAG",
		"-DCMAKE_C_COMPILER=$CC",
		"-DCMAKE_CXX_COMPILER=$CXX",
		"-DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH",
		"-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
		"-DBUILD_XFE_CONTROL_SIM_EXECUTABLE=OFF",
		"-DBUILD_SHARED_LIBS=ON"
	)
	$cmakeArgs += $PATHVARS

	& cmake @cmakeArgs
	if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

	# Build
	if ($UseNinja) {
		Push-Location $BUILD_DIR
		& $BUILD_CMD[0] $BUILD_CMD[1..($BUILD_CMD.Length-1)]
		$buildExit = $LASTEXITCODE
		Pop-Location
		if ($buildExit -ne 0) { exit $buildExit }
	} else {
		& $BUILD_CMD[0] $BUILD_CMD[1..($BUILD_CMD.Length-1)]
		if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
	}
}

Write-Host ""

# Ensure log dirs exist before running the binary
$logDir = Join-Path $SIM_EXAMPLE_DIR "log\log_data"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

# cd "$BUILD_DIR/executables-out/" || exit
$EXEC_OUT_DIR = Join-Path $BUILD_DIR "executables-out"
if (-not (Test-Path $EXEC_OUT_DIR)) { Write-Error "Executables directory not found: $EXEC_OUT_DIR"; exit 5 }
Set-Location $EXEC_OUT_DIR

# Determine binary
$BIN = "qblade_interface_test.exe"
if (-not (Test-Path $BIN)) { $BIN = "qblade_interface_test" }
if (-not (Test-Path $BIN)) { Write-Error "Built executable not found: $(Join-Path $EXEC_OUT_DIR 'qblade_interface_test[.exe]')"; exit 6 }

Write-Host "→ Running qblade interface test…"

# Run & capture output + exit (avoid NativeCommandError)
$stdoutPath = Join-Path $env:TEMP ("xflow_stdout_{0}.log" -f ([guid]::NewGuid().ToString("N")))
$stderrPath = Join-Path $env:TEMP ("xflow_stderr_{0}.log" -f ([guid]::NewGuid().ToString("N")))

$proc = Start-Process -FilePath ".\${BIN}" -NoNewWindow -Wait -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
$exitCode = $proc.ExitCode

# Echo stdout/stderr back to console
if (Test-Path $stdoutPath) { Get-Content -LiteralPath $stdoutPath | Write-Output }
if (Test-Path $stderrPath) { Get-Content -LiteralPath $stderrPath | Write-Output }

# Cleanup temp logs
Remove-Item $stdoutPath,$stderrPath -ErrorAction SilentlyContinue

if ($exitCode -ne 0) {
	Write-Error "❌ qblade interface test failed (exit $exitCode)"
	exit $exitCode
}

Write-Host "✅ qblade interface test passed!"

Write-Host ""
# Dump log if present
$LOG_FILE = Join-Path $logDir "xfe-control-sim-simulation-output.log"
if (Test-Path $LOG_FILE) {
	Write-Host "→ Contents of simulation log:"
	Get-Content -LiteralPath $LOG_FILE
	Write-Host ""
} else {
	Write-Warning "⚠️ Log file not found: $LOG_FILE"
}

exit $exitCode