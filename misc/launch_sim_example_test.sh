#!/usr/bin/env bash
# set -x
set -euo pipefail

# usage: ./misc/launch_sim_example_test.sh [rebuild?]
# if you pass "1" it will rm -rf and reconfigure, otherwise it will reuse.

XFLOW_CONTROL_SIM_ROOT=$(git rev-parse --show-toplevel)
SIM_EXAMPLE="${XFLOW_CONTROL_SIM_ROOT}/sim_example"
TMP_ROOT="$(dirname "${XFLOW_CONTROL_SIM_ROOT}")/sim_example_test"
BUILD_DIR="${TMP_ROOT}/build"

REBUILD=${1:-0}

echo "→ Testing sim_example in temporary dir: ${TMP_ROOT}"

# 1) recreate or skip
if [[ "$REBUILD" == "1" ]]; then
    rm -rf "$TMP_ROOT"
fi

if [[ ! -d "$TMP_ROOT" ]]; then
	echo "→ Copying sim_example to ${TMP_ROOT}"
	cp -R "$SIM_EXAMPLE" "$TMP_ROOT"
fi

# 2) configure & build
mkdir -p "$BUILD_DIR"

VERBOSE=1

# set number of procs based on OS
if [[ "$(uname)" == "Darwin" ]]; then
	NPROC=$(sysctl -n hw.ncpu)
else
	NPROC=$(nproc)
fi

# detect ninja vs. make
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

cmake $GENERATOR \
	-B "$BUILD_DIR" \
	-S "$TMP_ROOT" \
	-DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
	-DCMAKE_VERBOSE_MAKEFILE="$CMAKE_VERBOSE_FLAG" \
	-DCMAKE_C_COMPILER="$CC" \
	-DCMAKE_CXX_COMPILER="$CXX" \
	-DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
	-DBUILD_XFLOW_CONTROL_SIM_EXECUTABLE=ON \
	-DBUILD_SHARED_LIBS=OFF

echo "→ Building sim_example…"
# Actually build
(cd "$BUILD_DIR" && $BUILD_CMD)

# 3) run it
cd "$BUILD_DIR/executables-out/" || { echo "❌ Executables directory not found: $BUILD_DIR/executables-out" >&2; exit 1; }
BIN="xflow_control_sim"
if [[ -f "${BIN}.exe" ]]; then BIN="${BIN}.exe"; fi
if [[ ! -x "$BIN" ]]; then
	echo "❌ Built executable not found: $BUILD_DIR/executables-out/$BIN" >&2
	exit 1
fi

echo "→ Running sim_example test…"
OUTPUT="$(./"$BIN" 2>&1)"
EXIT_CODE=$?
if [[ $EXIT_CODE -ne 0 ]]; then
	echo "❌ sim_example failed (exit $EXIT_CODE)" >&2
	echo "$OUTPUT"
	# cleanup
	cd "$XFLOW_CONTROL_SIM_ROOT"
	rm -rf "$TMP_ROOT"
	exit $EXIT_CODE
fi

# 4) success
echo "✅ sim_example test passed!"
echo "$OUTPUT"

# 5) cleanup
cd "$XFLOW_CONTROL_SIM_ROOT"
rm -rf "$TMP_ROOT"

exit 0