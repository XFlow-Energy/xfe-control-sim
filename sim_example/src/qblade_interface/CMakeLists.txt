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

# sim_example/src/qblade_interface/CMakeLists.txt

#–– 1) collect your wrapper sources (they depend on xflow-control-sim-lib, which already
#     includes control_switch.c, xflow_control_sim_common.c, etc.)
set(QBLADE_INTERFACE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/discon.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/qblade_interface.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/qblade_control_switch.c"
)

target_sources(xflow-control-sim-lib PRIVATE ${QBLADE_INTERFACE_SOURCES})

#–– 2) build one executable that calls DISCON (test_discon.c has your main())
add_executable(qblade_interface_test
	test_discon.c
)

set_target_properties(qblade_interface_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/executables-out"
)

# on Windows you need kernel32 for LoadLibrary/GetProcAddress
if(WIN32)
	target_link_libraries(qblade_interface_test PRIVATE
		kernel32
	)
endif()

#–– 3) make sure the headers for both xflow-control-sim-lib and QBlade live where CMake can find them
target_include_directories(qblade_interface_test PRIVATE
    ${XFLOW_CONTROL_SIM_PROJECT_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

#–– 4) link against xflow-control-sim-lib (shared or static per BUILD_SHARED_LIBS) and xflow-utils
#     on UNIX also pull in libm for any math() calls
target_link_libraries(qblade_interface_test PRIVATE
    xflow-control-sim-lib
    xflow-utils
    $<IF:$<PLATFORM_ID:Windows>,,m>
)