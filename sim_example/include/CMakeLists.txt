# -----------------------------------------------------------------------------
# SPDX-License-Identifier: CC0-1.0
#
# This file is part of the XFLOW-CONTROL-SIM example suite.
#
# To the extent possible under law, XFlow Energy has waived all copyright
# and related or neighboring rights to this example file. This work is
# published from: United States.
#
# You should have received a copy of the CC0 Public Domain Dedication along
# with this software. If not, see <https://creativecommons.org/publicdomain/zero/1.0/>.
# -----------------------------------------------------------------------------

# sim/include/CMakeLists.txt

# Tell the xflow-control-sim-include INTERFACE target about these headers.
# We assume that the parent CMake called `add_subdirectory(sim_example)` before this.
# The `xflow-control-sim-include` target was created in include/CMakeLists.txt.
target_sources(xflow-control-sim-include INTERFACE
  FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
		drivetrains.h
		data_processing.h
		turbine_controls.h
		flow_sim_model.h
		equation_of_motion.h
		qblade_interface.h
		discon.h
)

# Also add this folder to the include path (so #include "drivetrains.h" works).
target_include_directories(xflow-control-sim-include INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
)