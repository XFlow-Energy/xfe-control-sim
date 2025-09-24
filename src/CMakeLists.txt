# -----------------------------------------------------------------------------
# SPDX-License-Identifier: GPL-3.0-or-later
#
# xfe-control-sim
# Copyright (C) 2024-2025 XFlow Energy (https://www.xflowenergy.com/)
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY and FITNESS for a particular purpose. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
# -----------------------------------------------------------------------------

# src/CMakeLists.txt
include_directories(${XFE_CONTROL_SIM_PROJECT_DIR}/include)

message(STATUS "XFE_CONTROL_SIM_LIB_COMPILE_DEFINITIONS = ${XFE_CONTROL_SIM_LIB_COMPILE_DEFINITIONS}")

set(git_commit_info_xfe_control_sim_warning "This only gets updated when CMake runs! Make sure you do a fresh CMake run before deploying to production.")
# Only run if this is a Git repo (e.g. top-level .git exists)
execute_process(
    COMMAND git rev-parse --is-inside-work-tree
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    RESULT_VARIABLE GIT_WORK_TREE_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if (GIT_WORK_TREE_RESULT EQUAL 0)
    execute_process(
        COMMAND git describe --all --long --dirty
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE git_commit_info_xfe_control_sim
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(git_commit_info_xfe_control_sim "unknown")
    message(WARNING "Git describe failed — not in a Git repo. Version info set to 'unknown'")
endif()

configure_file(xfe_control_sim_version.in.c xfe_control_sim_version.c @ONLY)

# -----------------------------------------------------------------------------
# xfe-control-sim-lib static library
# -----------------------------------------------------------------------------
list(APPEND LIB_SOURCES
	xfe_control_sim_common.c
	turbine_control_common.c
	xfe_control_sim_version.c
)

if(BUILD_XFE_CONTROL_SIM_EXECUTABLE OR BUILD_OTHER_PROJECT_INTEGRATION)
	list(APPEND LIB_SOURCES
		flow_gen.c
		numerical_integrator.c
		control_switch.c
	)
endif()

if(NOT TARGET xfe-control-sim-lib)
	add_library(xfe-control-sim-lib ${LIB_SOURCES})
endif()

if(EXISTS "${XFE_CONTROL_SIM_PROJECT_DIR}/include")
	target_include_directories(xfe-control-sim-lib PUBLIC
		"${XFE_CONTROL_SIM_PROJECT_DIR}/include"
		"${CMAKE_CURRENT_BINARY_DIR}"
	)
endif()

if(NOT WIN32)
	target_link_libraries(xfe-control-sim-lib PUBLIC xfe-control-sim-include xflow-utils libmodbus::modbus m)
else()
	target_link_libraries(xfe-control-sim-lib PUBLIC xfe-control-sim-include xflow-utils libmodbus::modbus)
endif()

if(INTEGRATE_CUSTOMER_MODELS)
	target_link_libraries(xfe-control-sim-lib PRIVATE aero-control-lib)
endif()

include(GenerateExportHeader)
generate_export_header(xfe-control-sim-lib
	EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/xfe_control_sim_lib_export.h
)

target_include_directories(xfe-control-sim-lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>
)

if(WIN32 AND BUILD_SHARED_LIBS)
	set_target_properties(xfe-control-sim-lib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

set_source_files_properties(
	xfe_control_sim_common.c
	${CUSTOM_XFE_CONTROL_SIM_FILES_ROOT}/src/data_processing.c
	PROPERTIES COMPILE_DEFINITIONS "${XFE_CONTROL_SIM_LIB_COMPILE_DEFINITIONS}"
)

set_source_files_properties(${CUSTOM_XFE_CONTROL_SIM_FILES_ROOT}/src/xfe_scada_interface.c PROPERTIES COMPILE_DEFINITIONS "${MODBUS_SERVER_COMPILE_DEFINITIONS}")

if(BUILD_OTHER_PROJECT_INTEGRATION)
	set_source_files_properties(
		xfe_control_sim_main.c
		flow_gen.c
		numerical_integrator.c
		PROPERTIES COMPILE_DEFINITIONS "${XFE_CONTROL_SIM_LIB_COMPILE_DEFINITIONS}"
	)
endif()
# -----------------------------------------------------------------------------
# xfe_control_sim executable
# -----------------------------------------------------------------------------
if(BUILD_XFE_CONTROL_SIM_EXECUTABLE)
	if(BUILD_XFE_SCADA_INTERFACE)
		set_source_files_properties(
			xfe_control_sim_main.c
			PROPERTIES COMPILE_DEFINITIONS "BUILD_XFE_SCADA_INTERFACE=1"
		)
	endif()

	set_source_files_properties(
		xfe_control_sim_main.c
		flow_gen.c
		numerical_integrator.c
		data_processing.c
		PROPERTIES COMPILE_DEFINITIONS "${XFE_CONTROL_SIM_LIB_COMPILE_DEFINITIONS}"
	)

	if(NOT TARGET xfe_control_sim)
		add_executable(xfe_control_sim xfe_control_sim_main.c)
	endif()
	
	# target_include_directories(xfe_control_sim PRIVATE
	# 	${XFE_CONTROL_SIM_PROJECT_DIR}/include
	# )

	target_link_libraries(xfe_control_sim PRIVATE xfe-control-sim-lib)

	if(WIN32)
		set(XFLOW_UTILS_DLL_DIR "${CMAKE_BINARY_DIR}/xflow-utils-windows")
		set(XFLOW_UTILS_DLL     "${XFLOW_UTILS_DLL_DIR}/libxflow-utils.dll")

		# copy it alongside the exe
		add_custom_command(TARGET xfe_control_sim POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"${XFLOW_UTILS_DLL}"
				$<TARGET_FILE_DIR:xfe_control_sim>
			COMMENT "Copying xflow-utils DLL to executable folder"
		)
	endif()
endif()

# -----------------------------------------------------------------------------
# modbus_server executable
# -----------------------------------------------------------------------------
if(BUILD_XFE_SCADA_INTERFACE)
	message(STATUS "MODBUS_SERVER_PROGRAM_COMPILE_DEFINITIONS = ${MODBUS_SERVER_COMPILE_DEFINITIONS}")
	set_source_files_properties(modbus_server.c PROPERTIES COMPILE_DEFINITIONS "${MODBUS_SERVER_PROGRAM_COMPILE_DEFINITIONS}")
	if(DEFINED MODBUS_SERVER_SOURCES)
		add_executable(modbus_server ${MODBUS_SERVER_SOURCES})
	else()
		add_executable(modbus_server modbus_server.c)
	endif()
	target_include_directories(modbus_server PRIVATE
		${XFE_CONTROL_SIM_PROJECT_DIR}/include
	)
	target_link_libraries(modbus_server PRIVATE xflow-utils)
endif()
