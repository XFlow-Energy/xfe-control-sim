#!/bin/bash
set -x
XFLOW_CONTROL_SIM_DIR=$(git rev-parse --show-toplevel)

BUILD_DIR="$XFLOW_CONTROL_SIM_DIR/build"

RECOMPILE_OR_NOT=$1

killall -9 -v xflow_control_sim

if [ "$RECOMPILE_OR_NOT" == 1 ]; then
	rm -r "$BUILD_DIR"
	mkdir "$BUILD_DIR"
	cd "$BUILD_DIR" || exit
 
	# Create the PATHVARS string with options
	# PATHVARS+="-DADDRESS_SANITIZER=ON "
	# PATHVARS+="-DRUN_FLAWFINDER=OFF "
	# PATHVARS+="-DRUN_CPPCHECK=OFF "
	# PATHVARS+="-DRUN_IWYU=ON "

	# Pass PATHVARS as separate arguments to cmake by using $PATHVARS inside the quotes
	cmake -DCMAKE_BUILD_TYPE=Release ${PATHVARS} ../src

	# Build the project
	# make VERBOSE=1 -j$(nproc)
	make -j"$(nproc)"
fi

cd "$BUILD_DIR/executables-out/" || exit

# Run the executable
# ./xflow_control_sim