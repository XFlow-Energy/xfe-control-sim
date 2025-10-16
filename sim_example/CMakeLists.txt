# -----------------------------------------------------------------------------
# SPDX-License-Identifier: CC0-1.0
#
# This file is part of the XFE-CONTROL-SIM example suite.
#
# To the extent possible under law, XFlow Energy has waived all copyright
# and related or neighboring rights to this example file. This work is
# published from: United States.
#
# You should have received a copy of the CC0 Public Domain Dedication along
# with this software. If not, see <https://creativecommons.org/publicdomain/zero/1.0/>.
# -----------------------------------------------------------------------------

# Load global config
# sim_example/CMakeLists.txt
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED PROJECT_IS_TOP_LEVEL)
    set(MY_PROJECT_REALLY_IS_TOP_LEVEL TRUE)
else()
	set(MY_PROJECT_REALLY_IS_TOP_LEVEL ${PROJECT_IS_TOP_LEVEL})
endif()

if (MY_PROJECT_REALLY_IS_TOP_LEVEL AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(xfe-control-sim VERSION 1.0 LANGUAGES C)

    set(CMAKE_C_STANDARD 23)
    set(CMAKE_CXX_STANDARD 20)

	message(STATUS "We are running sim_example as stand alone project")

	if (NOT DEFINED CUSTOM_XFE_CONTROL_SIM_FILES_ROOT)
		get_filename_component(CUSTOM_XFE_CONTROL_SIM_FILES_ROOT "${CMAKE_SOURCE_DIR}" ABSOLUTE)
	endif()

	set(XFE_CONTROL_SIM_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/config/simple_turbine_config.cmake" CACHE  PATH  "Which xfe_control_sim config to load" FORCE)

	include(${XFE_CONTROL_SIM_CONFIG_FILE})

	list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
	include(CMakeDependentOption) 

	set(XFE_CONTROL_SIM_VERSION "v0.0.4" CACHE STRING "Version of xfe-control-sim to use (empty = latest)")
	find_package(xfe-control-sim REQUIRED)

	# specifically for xflowenergy project set these values when testing on its own.
    set(BUILD_OTHER_PROJECT_INTEGRATION OFF CACHE BOOL "Forcing to build for sim here" FORCE)
else()
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "Do not build shared libs" FORCE)
	set(BUILD_OTHER_PROJECT_INTEGRATION ON CACHE BOOL "Forcing to build for other project integration" FORCE)
endif()

message(STATUS "WE HAVE ENTERED sim_example")

add_subdirectory(include)

target_sources(xfe-control-sim-lib PRIVATE
	src/drivetrain.c
	src/turbine_control.c
)

if(BUILD_XFE_CONTROL_SIM_EXECUTABLE OR BUILD_OTHER_PROJECT_INTEGRATION)
	target_sources(xfe-control-sim-lib PRIVATE
		src/flow_sim_model.c
		src/data_processing.c
		src/equation_of_motion.c
	)
endif()

target_include_directories(xfe-control-sim-lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# note, qblade_interface is added in xfe-control-sim. 

if(BUILD_SHARED_LIBS)
	add_subdirectory(src/qblade_interface)
endif()

if(LIBRARY_OUTPUT_DIR AND BUILD_SHARED_LIBS AND (WIN32 OR (UNIX AND NOT APPLE)))

	file(MAKE_DIRECTORY "${LIBRARY_OUTPUT_DIR}")

	# 1) A custom “all” target that depends on xfe-control-sim-lib
	add_custom_target(copy_xfe_control_sim_lib_for_${CUSTOMER_NAME}
	ALL
	DEPENDS xfe-control-sim-lib
	COMMENT "Copying & renaming xfe-control-sim-lib and discon.in for ${CUSTOMER_NAME}"
	)

	# 2) Post‐build steps: library + discon.in
	add_custom_command(TARGET copy_xfe_control_sim_lib_for_${CUSTOMER_NAME}
	POST_BUILD
		# copy & rename the shared library
		COMMAND ${CMAKE_COMMAND} -E copy
		"$<TARGET_FILE:xfe-control-sim-lib>"
		"${LIBRARY_OUTPUT_DIR}/libxfe-control-sim-lib-${CUSTOMER_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
		# copy & rename discon.in
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CMAKE_CURRENT_SOURCE_DIR}/src/qblade_interface/discon.in"
		"${LIBRARY_OUTPUT_DIR}/discon_${CUSTOMER_NAME}.in"
	)
endif()

message(STATUS "XFE_CONTROL_SIM_CONFIG_DIR = ${XFE_CONTROL_SIM_CONFIG_DIR}")
message(STATUS "XFE_CONTROL_SIM_CONFIG_FILE = ${XFE_CONTROL_SIM_CONFIG_FILE}")
message(STATUS "BUILD_SHARED_LIBS = ${BUILD_SHARED_LIBS}")
message(STATUS "BUILD_OTHER_PROJECT_INTEGRATION = ${BUILD_OTHER_PROJECT_INTEGRATION}")
message(STATUS "BUILD_XFE_CONTROL_SIM_EXECUTABLE = ${BUILD_XFE_CONTROL_SIM_EXECUTABLE}")
message(STATUS "BUILD_XFE_SCADA_INTERFACE = ${BUILD_XFE_SCADA_INTERFACE}")
message(STATUS "INTEGRATE_CUSTOMER_MODELS = ${INTEGRATE_CUSTOMER_MODELS}")
message(STATUS "CMAKE_SOURCE_DIR = ${CMAKE_SOURCE_DIR}")