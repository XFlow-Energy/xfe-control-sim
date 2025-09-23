#!/bin/bash
set -euo pipefail

PROJECT_ROOT="${PROJECT_ROOT:-$(pwd)}"
BUILD_DIR="${BUILD_DIR:-${PROJECT_ROOT}/build}"
RUN_CLANG_TIDY_BIN="${RUN_CLANG_TIDY_BIN:-run-clang-tidy}"
MODE="${1:-all}"

cd "${PROJECT_ROOT}"

if [[ "$(uname)" == "Darwin" ]]; then NPROC=$(sysctl -n hw.ncpu); else NPROC=$(nproc); fi

# macOS-only SDK arg
EXTRA_ARGS=("-j $NPROC" "-p=${BUILD_DIR}")
if [ "$(uname -s)" = "Darwin" ]
then
	SDK_PATH="$(xcrun --show-sdk-path 2>/dev/null || true)"
	if [ -n "${SDK_PATH}" ]
	then
		EXTRA_ARGS+=("-extra-arg=-isysroot${SDK_PATH}")
	fi
fi

# Build file list by mode
FILES=()
if [ "${MODE}" = "c" ]
then
	while IFS= read -r -d '' f
	do
		FILES+=("$f")
	done < <(find . -type f -name '*.c' -print0)

elif [ "${MODE}" = "cpp" ]
then
	while IFS= read -r -d '' f
	do
		FILES+=("$f")
	done < <(find . -type f \( -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' \) -print0)
elif [ "${MODE}" = "both" ] || [ "${MODE}" = "all" ]
then
	while IFS= read -r -d '' f
	do
		FILES+=("$f")
	done < <(find . -type f \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' \) -print0)
else
	echo "usage: $(basename "$0") [c|cpp|both|all]"; exit 2
fi

echo "[clang-tidy] ${RUN_CLANG_TIDY_BIN} ${EXTRA_ARGS[*]} ${#FILES[@]} files"
exec "${RUN_CLANG_TIDY_BIN}" "${EXTRA_ARGS[@]}" "${FILES[@]}"