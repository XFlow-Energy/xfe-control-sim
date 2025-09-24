#!/usr/bin/env bash
# set -x
set -euo pipefail

# usage: ./misc/launch_discon_test.sh [rebuild?]
# if you pass "1" it will rm -rf and reconfigure, otherwise it will reuse.

XFE_CONTROL_SIM_ROOT=$(git rev-parse --show-toplevel)
SIM_EXAMPLE="${XFE_CONTROL_SIM_ROOT}/sim_example"
TMP_ROOT="$(dirname "${XFE_CONTROL_SIM_ROOT}")/sim_example_test"

REBUILD=${1:-0}

echo "→ Testing DISCON in temporary dir: ${TMP_ROOT}"

# 1) recreate or skip
if [[ "$REBUILD" == "1" ]]; then
	rm -rf "$TMP_ROOT"
fi

if [[ ! -d "$TMP_ROOT" ]]; then
	echo "→ Copying sim_example to ${TMP_ROOT}"
	cp -R "$SIM_EXAMPLE" "$TMP_ROOT"
fi

# 2) Build + run via the inner launcher (run from repo so git works)
echo "→ Building + running via sim_example_test/misc/launch_discon.sh (RECOMPILE_OR_NOT=${REBUILD})"
(
	cd "${TMP_ROOT}/misc"
	./launch_discon.sh "${REBUILD}"
)
LAUNCH_EXIT=$?

# 3) outcome
if [[ $LAUNCH_EXIT -ne 0 ]]; then
	echo "❌ DISCON test run failed (exit $LAUNCH_EXIT)" >&2
else
	echo "✅ DISCON test run succeeded"
fi

# 4) validate log file contents before cleanup
LOG_FILE="${TMP_ROOT}/log/log_data/xfe-control-sim-simulation-output.log"
echo "→ Validating log file: ${LOG_FILE}"

LOG_OK=1
if [[ ! -f "$LOG_FILE" ]]; then
	echo "❌ Log file not found." >&2
	LOG_OK=0
else
	if ! grep -Fq "discon init complete!" "$LOG_FILE"; then
		echo "❌ Missing 'Program Duration:' line." >&2
		LOG_OK=0
	fi
	if grep -Fq "ERROR" "$LOG_FILE"; then
		echo "❌ Found error lines in log:" >&2
		grep -F "ERROR" "$LOG_FILE" >&2
		LOG_OK=0
	fi
fi

cd "$XFE_CONTROL_SIM_ROOT"

if [[ $LOG_OK -eq 1 ]]; then
	echo "✅ Log validation passed. Cleaning up temp folder: ${TMP_ROOT}"
	rm -rf "$TMP_ROOT"
	exit $LAUNCH_EXIT
else
	echo "⚠️  Log validation failed. Preserving temp folder for inspection: ${TMP_ROOT}" >&2
	echo "   You can inspect the log with: less '${LOG_FILE}'" >&2
	# If the run itself failed, propagate its code; otherwise signal validation failure.
	if [[ $LAUNCH_EXIT -ne 0 ]]; then
		exit $LAUNCH_EXIT
	else
		exit 1
	fi
fi
