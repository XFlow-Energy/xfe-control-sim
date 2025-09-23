#!/bin/bash
set -x
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIM_EXAMPLE_DIR="$(cd "$SCRIPT_DIR/../" && pwd)"

XFLOW_CONTROL_SIM_DIR="$(cd "$SIM_EXAMPLE_DIR/../xflow-control-sim" && pwd)"

BUILD_DIR="$SIM_EXAMPLE_DIR/build"

RECOMPILE_OR_NOT=$1

if [ "$RECOMPILE_OR_NOT" == 1 ]; then
	rm -rf "$BUILD_DIR"
	rm -rf ~/.cache/cppcheck
	mkdir "$BUILD_DIR"
	cd "$BUILD_DIR" || exit
	# PATHVARS+="-DRUN_CPPCHECK=OFF "
	# PATHVARS+="-DRUN_IWYU=OFF "
	# PATHVARS+="-DRUN_CLANG_TIDY=OFF "
	# PATHVARS+="-DRUN_SCAN_BUILD=OFF "
	# PATHVARS+="-DRUN_FLAWFINDER=OFF "

	VERBOSE=1

	# set number of procs based on OS
	if [[ "$(uname)" == "Darwin" ]]; then
		NPROC=$(sysctl -n hw.ncpu)
	else
		NPROC=$(nproc)
	fi

	# detect ninja vs make
	if command -v ninja >/dev/null 2>&1; then
		GENERATOR="-G Ninja"
		if [[ $VERBOSE -eq 1 ]]; then
			BUILD_CMD="ninja -v -j $NPROC"
		else
			BUILD_CMD="ninja -j $NPROC"
		fi
	else
		GENERATOR=""
		if [[ $VERBOSE -eq 1 ]]; then
			BUILD_CMD="make -j$NPROC VERBOSE=1"
		else
			BUILD_CMD="make -j$NPROC"
		fi
	fi

	if [[ "${GITHUB_ACTIONS:-}" == "true" ]]; then
		OS="${RUNNER_OS}"
	else
		OS="$(uname -s)"
	fi

	# pick compilers (same logic as your other script + CI overrides)
	if [[ "${GITHUB_ACTIONS:-}" == "true" ]]; then
	# on GH-Actions, use RUNNER_OS or uname to decide
	case "$OS" in
		Windows)
			CC="C:/deps/llvm-mingw/bin/clang.exe"
			CXX="C:/deps/llvm-mingw/bin/clang++.exe"
			;;
		macOS|Darwin)
			CC="/opt/homebrew/opt/llvm/bin/clang"
			CXX="/opt/homebrew/opt/llvm/bin/clang++"
			;;
		Linux)
			CC="/usr/bin/clang"
			CXX="/usr/bin/clang++"
			;;
		*)
			echo "❌ Unsupported OS: $OS" >&2
			exit 1
			;;
	esac
	else
	# local/non-CI build
	case "$(uname -s)" in
		Darwin)
		CC="/opt/homebrew/opt/llvm/bin/clang"
		CXX="/opt/homebrew/opt/llvm/bin/clang++"
		;;
		*)
		CC="clang"
		CXX="clang++"
		;;
	esac
	fi

	# turn CMake’s verbose-makefile on/off
	if [[ "${VERBOSE:-0}" -eq 1 ]]; then
		CMAKE_VERBOSE_FLAG=ON
	else
		CMAKE_VERBOSE_FLAG=OFF
	fi

	# CI‐only prefix path for Windows dependencies
	CMAKE_PREFIX_PATH=""
	if [[ "${GITHUB_ACTIONS:-}" == "true" && "$OS" == "Windows" ]]; then
		CMAKE_PREFIX_PATH="C:/deps/gsl-install;C:/deps/jansson-install;C:/deps/libmodbus"
	fi

	# build type
	if [[ "${GITHUB_ACTIONS:-}" == "true" ]]; then
		BUILD_TYPE="Release"
	else
		BUILD_TYPE="Debug"
	fi
	BUILD_TYPE="Release"

	export CC CXX CMAKE_VERBOSE_FLAG CMAKE_PREFIX_PATH BUILD_TYPE
	# Configure into build dir for DISCON test (no main sim executable; shared libs ON)
	cmake $GENERATOR \
		-B "$BUILD_DIR" \
		-S "$SIM_EXAMPLE_DIR" \
		-DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
		-DCMAKE_VERBOSE_MAKEFILE="$CMAKE_VERBOSE_FLAG" \
		-DCMAKE_C_COMPILER="$CC" \
		-DCMAKE_CXX_COMPILER="$CXX" \
		-DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DBUILD_XFLOW_CONTROL_SIM_EXECUTABLE=OFF \
		-DBUILD_SHARED_LIBS=ON

	# Actually build
	(cd "$BUILD_DIR" && $BUILD_CMD)
fi

RUN_CLANG_TIDY=1
# Post-build: run clang-tidy via your script and log to build/clang-tidy.log
if [[ "${RUN_CLANG_TIDY:-0}" == "1" ]]; then
	CLANG_TIDY_SCRIPT="$XFLOW_CONTROL_SIM_DIR/misc/clang_tidy_all.sh"
	CLANG_TIDY_LOG="$BUILD_DIR/clang-tidy.log"

	# Allow user override of .clang-tidy file location
	CLANG_TIDY_FILE="${CLANG_TIDY_FILE:-$XFLOW_CONTROL_SIM_DIR/.clang-tidy}"

	if [[ -x "$CLANG_TIDY_SCRIPT" ]]; then
		if command -v run-clang-tidy >/dev/null 2>&1; then
			export PROJECT_ROOT="$SIM_EXAMPLE_DIR"
			export BUILD_DIR="$BUILD_DIR"
			export RUN_CLANG_TIDY_BIN="$(command -v run-clang-tidy)"
			MODE="${RUN_CLANG_TIDY_MODE:-c}"

			# remove existing log if present
			if [[ -f "$CLANG_TIDY_LOG" ]]; then
				rm -f "$CLANG_TIDY_LOG"
			fi

			echo "[INFO] Running clang-tidy ($MODE)… logging to $CLANG_TIDY_LOG"
			echo "[INFO] Using config file: $CLANG_TIDY_FILE"

			# send output to both console and file
			if ! "$CLANG_TIDY_SCRIPT" "$MODE" "-config-file=$CLANG_TIDY_FILE" 2>&1 | tee "$CLANG_TIDY_LOG"; then
				echo "[WARN] clang-tidy returned non-zero; see $CLANG_TIDY_LOG"
			fi
		else
			echo "[WARN] run-clang-tidy not found in PATH; skipping clang-tidy step."
		fi
	else
		echo "[WARN] clang-tidy script not found at $CLANG_TIDY_SCRIPT; skipping."
	fi
else
	echo "[INFO] RUN_CLANG_TIDY not set; skipping clang-tidy step."
fi

echo ""

# Ensure log dirs exist before running the binary (CI needs this)
mkdir -p "$SIM_EXAMPLE_DIR/log/log_data"

cd "$BUILD_DIR/executables-out/" || { echo "❌ Executables directory not found: $BUILD_DIR/executables-out" >&2; exit 1; }

BIN="qblade_interface_test"
if [[ -f "${BIN}.exe" ]]; then BIN="${BIN}.exe"; fi
if [[ ! -x "$BIN" ]]; then
	echo "❌ Built executable not found: $BUILD_DIR/executables-out/$BIN" >&2
	exit 1
fi

echo "→ Running qblade interface test…"
OUTPUT="$(./"$BIN" 2>&1)"
EXIT_CODE=$?

if [[ $EXIT_CODE -ne 0 ]]; then
	echo "❌ qblade interface test failed (exit $EXIT_CODE)" >&2
	echo "$OUTPUT"
	exit $EXIT_CODE
fi

echo "✅ qblade interface test passed!"
echo "$OUTPUT"

echo ""

# Print the contents of the generated log file if it exists
LOG_FILE="$SIM_EXAMPLE_DIR/log/log_data/xflow-control-sim-simulation-output.log"
if [[ -f "$LOG_FILE" ]]; then
	echo "→ Contents of simulation log:"
	cat "$LOG_FILE"
	echo ""
else
	echo "⚠️ Log file not found: $LOG_FILE"
fi

exit 0