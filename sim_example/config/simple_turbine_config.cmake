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

# config/config.cmake
# Define paths to the source files and configurations

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

option(BUILD_OTHER_PROJECT_INTEGRATION "Allow for main files to be compiled int o ther projects. no build executable or shared lib." OFF)

# Option to build the xfe_control_sim executable
option(BUILD_XFE_CONTROL_SIM_EXECUTABLE "Build xfe_control_sim executable" ON)

option(BUILD_XFE_SCADA_INTERFACE "Build xfe_scada interface" OFF)

option(INTEGRATE_CUSTOMER_MODELS "Build custom customer models" OFF)

option(RUN_SINGLE_MODEL_ONLY "Run only a single instance of the program, otherwise allows for data processing functions." ON)

set(CUSTOMER_NAME "xflowenergy" CACHE STRING "Name of the customer to use")
set(GIT_TAG_TO_USE "add_xfe_control_sim" CACHE STRING "git tag to use")

# Adjust BUILD_SHARED_LIBS
if(BUILD_SHARED_LIBS AND BUILD_XFE_CONTROL_SIM_EXECUTABLE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries on Windows when building the executable" FORCE)
endif()

if(BUILD_XFE_SCADA_INTERFACE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Forcing windows shared library off just in case" FORCE)
    set(BUILD_XFE_CONTROL_SIM_EXECUTABLE ON CACHE BOOL "Forcing on building the xfe-control-sim executable" FORCE)
endif()

if(BUILD_OTHER_PROJECT_INTEGRATION)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Forcing windows shared library off just in case" FORCE)
    set(BUILD_XFE_CONTROL_SIM_EXECUTABLE OFF CACHE BOOL "Forcing on building the xfe-control-sim executable" FORCE)
endif()

# Option to specify the library output directory
set(LIBRARY_OUTPUT_DIR "" CACHE PATH "Directory to copy the built library to")

# Get the current username
if(WIN32)
    set(CURRENT_USER $ENV{USERNAME})
else()
    set(CURRENT_USER $ENV{USER})
endif()

message(STATUS "Current user: '${CURRENT_USER}'")

# Set LIBRARY_OUTPUT_DIR based on username
if(WIN32)
    if("${CURRENT_USER}" STREQUAL "XFlow Sim")
        set(LIBRARY_OUTPUT_DIR "C:/Users/XFlow Sim/Documents/QBlade Repo/QBladeEE_2.0.7.7/ControllerFiles" CACHE PATH "Directory to copy the built library to" FORCE)
    elseif("${CURRENT_USER}" STREQUAL "Field")
        set(LIBRARY_OUTPUT_DIR "C:/Users/Field/Downloads/QBladeCE_2.0.7.7_win/QBladeCE_2.0.7.7/ControllerFiles" CACHE PATH "Directory to copy the built library to" FORCE)
    elseif("${CURRENT_USER}" STREQUAL "xflow")
        set(LIBRARY_OUTPUT_DIR "C:/Users/xflow/Downloads/QBladeCE_2.0.7.7_win/QBladeCE_2.0.7.7/ControllerFiles" CACHE PATH "Directory to copy the built library to" FORCE)
    endif()
endif()

# Conditionally set options based on the current user
if("${CURRENT_USER}" STREQUAL "XFlow Sim" AND NOT BUILD_XFE_CONTROL_SIM_EXECUTABLE)
    # Default to building shared libraries and not building the executable
    set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)
    set(BUILD_XFE_CONTROL_SIM_EXECUTABLE OFF CACHE BOOL "Build xfe_control_sim executable" FORCE)
else()
    # Default options for other users
    option(BUILD_SHARED_LIBS "Build shared libraries" ON)
    option(BUILD_XFE_CONTROL_SIM_EXECUTABLE "Build xfe_control_sim executable" ON)
endif()

set(SYSTEM_CONFIG_FILENAME "simple_turbine_config.csv")

set(MODBUS_NETWORK_FILENAME "modbus_network")

set(DYNAMIC_DATA_EXPORT_FILENAME "dynamic_data_export")
set(FIXED_DATA_EXPORT_FILENAME "fixed_data_export")

set(DATA_PROCESSING_EXPORT_FILENAME "data_processing_data_export")

set(FLOW_GEN_DIR "${CMAKE_CURRENT_LIST_DIR}/flow")

if(NOT DEFINED FLOW_GEN_FILE_DIR)
    set(FLOW_GEN_FILE_DIR
        ${FLOW_GEN_DIR}
        CACHE PATH "Path to CSV file" FORCE
    )
endif()

set(XFE_CONTROL_SIM_CONFIG_DIR "${CMAKE_CURRENT_LIST_DIR}")

# Paths to config files
if(NOT DEFINED XFE_CONTROL_SIM_CONFIG_DIR)
    set(XFE_CONTROL_SIM_CONFIG_DIR
        "${CUSTOM_XFE_CONTROL_SIM_FILES_ROOT}/../../conf/turbineconfigfiles"
        CACHE PATH "Which xfe_control_sim config to load")
endif()

set(LOG_DIR "${CMAKE_SOURCE_DIR}/log")
set(OUTPUT_LOG_FILE_PATH "${LOG_DIR}")

if(RUN_SINGLE_MODEL_ONLY)
    set(LOGGING_DYNAMIC_DATA_CONTINUOUS_VALUE 1)
else()
    set(LOGGING_DYNAMIC_DATA_CONTINUOUS_VALUE 0)
endif()

# Compile definitions that are always included
set(XFE_CONTROL_SIM_LIB_COMPILE_DEFINITIONS
    SYSTEM_CONFIG_FULL_PATH="${XFE_CONTROL_SIM_CONFIG_DIR}/${SYSTEM_CONFIG_FILENAME}"
    DYNAMIC_DATA_FULL_PATH="${LOG_DIR}/${DYNAMIC_DATA_EXPORT_FILENAME}.csv"
    FIXED_DATA_FULL_PATH="${LOG_DIR}/${FIXED_DATA_EXPORT_FILENAME}.csv"
    DATA_PROCESSING_FULL_PATH="${LOG_DIR}/${DATA_PROCESSING_EXPORT_FILENAME}.csv"
    LOGGING_DYNAMIC_DATA_CONTINUOUS=${LOGGING_DYNAMIC_DATA_CONTINUOUS_VALUE}
    RUN_SINGLE_MODEL_ONLY=${RUN_SINGLE_MODEL_ONLY}
    OUTPUT_LOG_FILE_PATH="${OUTPUT_LOG_FILE_PATH}"
    DELETE_LOG_FILE_NEW_RUN=1 # set to 0 to keep history
)

set(MODBUS_DEVICE_TYPE "\"2\"" CACHE STRING "Modbus device type")
set(MODBUS_DEV_NUM "\"2\"" CACHE STRING "Modbus device number")
set(MODBUS_SERVER_IP "\"192.168.67.1\"" CACHE STRING "Modbus server IP address")
set(MODBUS_TCP_PORT "\"1503\"" CACHE STRING "Modbus TCP port")
set(MODBUS_TIMEOUT_US "\"100000\"" CACHE STRING "Modbus timeout in microseconds")

set(MODBUS_SERVER_COMPILE_DEFINITIONS
    MODBUS_SERVER_EXECUTABLE_FULL_PATH=\"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${MODBUS_SERVER_FILENAME}\"
    MODBUS_NETWORK_FULL_PATH="${XFE_CONTROL_SIM_CONFIG_DIR}/${MODBUS_NETWORK_FILENAME}.csv"
    MODBUS_DEVICE_FULL_PATH="${XFE_CONTROL_SIM_CONFIG_DIR}"
    MODBUS_DEVICE_TYPE=${MODBUS_DEVICE_TYPE}
    MODBUS_DEV_NUM=${MODBUS_DEV_NUM}
    MODBUS_SERVER_IP=${MODBUS_SERVER_IP}
    MODBUS_TCP_PORT=${MODBUS_TCP_PORT}
    MODBUS_TIMEOUT_US=${MODBUS_TIMEOUT_US}
    EXECUTABLES_DIR=\"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\"
    OUTPUT_LOG_FILE_PATH="${OUTPUT_LOG_FILE_PATH}"
)

set(MODBUS_SERVER_PROGRAM_COMPILE_DEFINITIONS
    OUTPUT_LOG_FILE_PATH="${OUTPUT_LOG_FILE_PATH}"
)

# Conditional inclusion for shared libraries (BUILD_SHARED_LIBS is ON)
if(NOT BUILD_SHARED_LIBS)
    list(APPEND XFE_CONTROL_SIM_LIB_COMPILE_DEFINITIONS
        FLOW_GEN_FILE_DIR="${FLOW_GEN_FILE_DIR}"
    )
endif()

if(BUILD_OTHER_PROJECT_INTEGRATION)
    list(APPEND XFE_CONTROL_SIM_LIB_COMPILE_DEFINITIONS
        FLOW_RUN_AFTER_END="1"
    )
endif()
