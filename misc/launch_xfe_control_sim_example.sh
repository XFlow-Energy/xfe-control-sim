#!/bin/bash
# set -x
XFE_CONTROL_SIM_DIR=$(git rev-parse --show-toplevel)

BUILD_DIR="$XFE_CONTROL_SIM_DIR/build"

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

	# pick compilers (same logic as your main script + CI overrides)
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

	if [[ "${GITHUB_ACTIONS:-}" == "true" ]]; then
		BUILD_TYPE="Release"
	else
		BUILD_TYPE="Debug"
	fi

	export CC CXX CMAKE_VERBOSE_FLAG CMAKE_PREFIX_PATH BUILD_TYPE
	# Configure into build dir
	cmake $GENERATOR \
		-B "$BUILD_DIR" \
		-S "$XFE_CONTROL_SIM_DIR" \
		-DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
		-DCMAKE_VERBOSE_MAKEFILE="$CMAKE_VERBOSE_FLAG" \
		-DCMAKE_C_COMPILER="$CC" \
		-DCMAKE_CXX_COMPILER="$CXX" \
		-DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DBUILD_XFE_CONTROL_SIM_EXECUTABLE=ON \
		-DBUILD_SHARED_LIBS=OFF \
		${PATHVARS}

	# Actually build
	(cd "$BUILD_DIR" && $BUILD_CMD)
fi

RUN_CLANG_TIDY=1

if [[ "${GITHUB_ACTIONS:-}" == "true" || -n "${CI:-}" ]]
then
	RUN_CLANG_TIDY=1
fi
# Post-build: run clang-tidy via your script and log to build/clang-tidy.log
if [[ "${RUN_CLANG_TIDY:-0}" == "1" ]]; then
	CLANG_TIDY_SCRIPT="$XFE_CONTROL_SIM_DIR/misc/clang_tidy_all.sh"
	CLANG_TIDY_LOG="$BUILD_DIR/clang-tidy.log"

	# Allow user override of .clang-tidy file location
	CLANG_TIDY_FILE="${CLANG_TIDY_FILE:-$XFE_CONTROL_SIM_DIR/.clang-tidy}"

	# Detect "cloud" (GitHub Actions or generic CI env)
	IN_CLOUD=0
	if [[ "${GITHUB_ACTIONS:-}" == "true" || -n "${CI:-}" ]]
	then
		IN_CLOUD=1
	fi

	if [[ -x "$CLANG_TIDY_SCRIPT" ]]; then
		if command -v run-clang-tidy >/dev/null 2>&1; then
			export PROJECT_ROOT="$XFE_CONTROL_SIM_DIR"
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
			if [[ "$IN_CLOUD" -eq 1 ]]
			then
				if ! "$CLANG_TIDY_SCRIPT" "$MODE" "$CLANG_TIDY_FILE" --extraargs -warnings-as-errors='*' 2>&1 | tee "$CLANG_TIDY_LOG"; then
					echo "[WARN] clang-tidy returned non-zero; see $CLANG_TIDY_LOG"
				fi
			else
				if ! "$CLANG_TIDY_SCRIPT" "$MODE" "$CLANG_TIDY_FILE" 2>&1 | tee "$CLANG_TIDY_LOG"; then
					echo "[WARN] clang-tidy returned non-zero; see $CLANG_TIDY_LOG"
				fi
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
mkdir -p "$XFE_CONTROL_SIM_DIR/log/log_data"

cd "$BUILD_DIR/executables-out/" || exit

#Determine correct binary name across platforms
BIN="xfe_control_sim"
if [[ -f "${BIN}.exe" ]]; then BIN="${BIN}.exe"; fi

# Run it and capture output + exit code
OUTPUT="$(./"$BIN" 2>&1)"
EXIT_CODE=$?
# Echo stdout/stderr and propagate exit code
echo "$OUTPUT"
echo ""

# Print the contents of the generated log file if it exists
LOG_FILE="$XFE_CONTROL_SIM_DIR/log/log_data/xfe-control-sim-simulation-output.log"
if [[ -f "$LOG_FILE" ]]; then
	echo "→ Contents of simulation log:"
	cat "$LOG_FILE"
	echo ""
else
	echo "⚠️ Log file not found: $LOG_FILE"
fi

# Validate log contents (must include Program Duration:, write Duration:, and end with Closing Program)
FINAL_EXIT=$EXIT_CODE
if [[ -f "$LOG_FILE" ]]; then
	LOG_OK=1
	if ! grep -Fq "Program Duration:" "$LOG_FILE"; then
		echo "❌ Missing 'Program Duration:' line in log."
		LOG_OK=0
	fi
	if ! grep -Fq "write Duration:" "$LOG_FILE"; then
		echo "❌ Missing 'write Duration:' line in log."
		LOG_OK=0
	fi
	LAST_NONEMPTY_LINE="$(awk 'NF{last=$0} END{print last}' "$LOG_FILE")"
	if [[ "$LAST_NONEMPTY_LINE" != *"Closing Program"* ]]; then
		echo "❌ Last non-empty line is not 'Closing Program'."
		echo "   Last line was: ${LAST_NONEMPTY_LINE}"
		LOG_OK=0
	fi
	if [[ $LOG_OK -eq 0 && $FINAL_EXIT -eq 0 ]]; then
		FINAL_EXIT=1
	fi
else
	echo "❌ Cannot validate: log file missing."
	if [[ $FINAL_EXIT -eq 0 ]]; then
		FINAL_EXIT=1
	fi
fi

if [[ $LOG_OK -eq 1 ]]; then
	echo "✅ Log validation passed"
fi

exit $FINAL_EXIT