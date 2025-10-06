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

# xfe-control-sim/CMakeLists.txt
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED PROJECT_IS_TOP_LEVEL)
    set(MY_PROJECT_REALLY_IS_TOP_LEVEL TRUE)
else()
	set(MY_PROJECT_REALLY_IS_TOP_LEVEL PROJECT_IS_TOP_LEVEL)
endif()

# Only call project() if not already defined (i.e., not being included as a subproject)
if (MY_PROJECT_REALLY_IS_TOP_LEVEL AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(xfe-control-sim VERSION 1.0 LANGUAGES C)

    set(CMAKE_C_STANDARD 23)
    set(CMAKE_CXX_STANDARD 20)
	message(STATUS "project is top level: ${MY_PROJECT_REALLY_IS_TOP_LEVEL}")
else()
	message(STATUS "project is NOT top level: ${MY_PROJECT_REALLY_IS_TOP_LEVEL}")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(XFE_CONTROL_SIM_PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# figure out “this” project’s root (the folder containing config/)
if (NOT DEFINED XFE_CONTROL_SIM_ROOT)
    get_filename_component(XFE_CONTROL_SIM_ROOT "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
endif()
message(STATUS "XFE_CONTROL_SIM_ROOT: ${XFE_CONTROL_SIM_ROOT}")

if (NOT DEFINED CUSTOM_XFE_CONTROL_SIM_FILES_ROOT)
    get_filename_component(CUSTOM_XFE_CONTROL_SIM_FILES_ROOT "${CMAKE_CURRENT_LIST_DIR}/sim_example/" ABSOLUTE)
endif()

message(STATUS "CUSTOM_XFE_CONTROL_SIM_FILES_ROOT: ${CUSTOM_XFE_CONTROL_SIM_FILES_ROOT}")


if (MY_PROJECT_REALLY_IS_TOP_LEVEL AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	# Include the configuration (after setting output directories)
	# pick the config file from a cache variable, defaulting to the “stock” one
	if (NOT DEFINED XFE_CONTROL_SIM_CONFIG_FILE)
		set(XFE_CONTROL_SIM_CONFIG_FILE
			"${CUSTOM_XFE_CONTROL_SIM_FILES_ROOT}/config/simple_ball_config.cmake"
			CACHE  PATH  "Which xfe_control_sim config to load")
	endif()

	message(STATUS "XFE_CONTROL_SIM_CONFIG_FILE: ${XFE_CONTROL_SIM_CONFIG_FILE}")

	include(${XFE_CONTROL_SIM_CONFIG_FILE})
endif()

set(XFLOW_UTILS_VERSION "v0.0.42" CACHE STRING "Version of xflow-utils to use (empty = latest)")

# Add custom modules directory to CMake module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# Include necessary modules
include(CMakeDependentOption)

# Options
option(ADDRESS_SANITIZER "Enable address sanitizer?" OFF)
option(RUN_CPPCHECK "Run CPPcheck when compiling." OFF)
option(RUN_CLANG_TIDY "Run clang_tidy built into clang when compiling." OFF)
option(RUN_SCAN_BUILD "Run scan_build built into clang when compiling." OFF)
option(RUN_IWYU "Run Include What You Use when compiling." OFF)
option(RUN_FLAWFINDER "Run flawfinder when compiling." OFF) # Option for flawfinder
option(DETECT_CCACHE "Use ccache to speed up builds." OFF)

# Allow choosing static vs shared, but force static off on Apple platforms
option(XFLOW_UTILS_USE_STATIC "Link against the static xflow-utils library (no shared-lib needed at runtime)" ON)

if(APPLE OR (BUILD_SHARED_LIBS AND NOT WIN32))
	set(XFLOW_UTILS_USE_STATIC OFF CACHE BOOL "Link against the static xflow-utils library (no shared-lib needed at runtime)" FORCE)
endif()

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug)
endif()

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
	message(FATAL_ERROR "Unsupported CMAKE_BUILD_TYPE '${CMAKE_BUILD_TYPE}'. Only 'Debug' or 'Release' are supported. Please fix or update xflow-utils.")
endif()

# Detect Windows platform and set 64-bit architecture flags
if(WIN32)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Compiling for Windows with GCC/Clang.")

        # Ensure 64-bit compilation
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
        
        # Optionally, enable other Windows-specific flags
        add_compile_options(-DWIN32 -D_WINDOWS -Wall -Wextra -Wpedantic)
        add_link_options(-m64)
    endif()
endif()

# Set output directories
set(EXECUTABLES_DIR "executables-out" CACHE PATH "Where to put the finished executable files.")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${EXECUTABLES_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include ccache detection if enabled
include(cmake/detect_ccache.cmake)

# Compile dependencies if needed without warnings and linters
include(cmake/dependencies.cmake)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    message(STATUS "added add_compile_options(-Wall -Wextra -Wpedantic)")
endif()

# Check if the compiler supports -ffunction-sections and -fdata-sections
include(CheckCCompilerFlag)
check_c_compiler_flag("-ffunction-sections" COMPILER_SUPPORTS_FUNCTION_SECTIONS)
check_c_compiler_flag("-fdata-sections" COMPILER_SUPPORTS_DATA_SECTIONS)

if(COMPILER_SUPPORTS_FUNCTION_SECTIONS AND COMPILER_SUPPORTS_DATA_SECTIONS)
    # Add function/data sections for garbage collection in GCC/Clang
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        if(APPLE)
            # For macOS, use the macOS-specific linker flag
            add_link_options(-Wl,-dead_strip)
        else()
            # Only add function/data sections without gc-sections for non-macOS platforms
            add_compile_options(-ffunction-sections -fdata-sections)
        endif()
    endif()
else()
    message(WARNING "The current toolchain does not support function/data sections for garbage collection.")
endif()

# Address sanitizer options
if (ADDRESS_SANITIZER)
    if (APPLE)
        add_compile_options(-fsanitize=address -g2)
    else()
        add_compile_options(-fsanitize=address -ggdb3)
    endif()
    add_link_options(-fsanitize=address)
endif()

get_target_property(XFLOW_UTILS_INCLUDE_DIR xflow-utils-include INTERFACE_INCLUDE_DIRECTORIES)

# Cppcheck IWYU, flawfinder integration (skips Windows)
if (NOT WIN32)
    # CPPCheck integration
    if (RUN_CPPCHECK)
        find_program(CPPCHECK_EXECUTABLE cppcheck)
        if (CPPCHECK_EXECUTABLE)
			message(STATUS "CPPCHECK_EXECUTABLE.")
            # Suppress the "normalCheckLevelMaxBranches" warning (if it ever fires)
            # and also silence unmatched-suppression notices
            set(CPPCHECK_SUPPRESS
                "--suppress=normalCheckLevelMaxBranches"
                "--suppress=unmatchedSuppression"
				"--suppress=missingIncludeSystem"
            )

            # Base arguments for Cppcheck
            #  --force      : check all configurations, even past the default limit
            #  --enable=warning,style,information : include informational messages
            #  --inline-suppr : honor in-source suppressions
			# after — explicitly enable missing-include checks:
			set(CPPCHECK_BASE_ARGS
				--force
				--enable=warning,style,information
				--inline-suppr
				${CPPCHECK_SUPPRESS}
				"--suppress=*:${FETCHCONTENT_BASE_DIR}/*"
				-I${XFLOW_UTILS_INCLUDE_DIR}
			)

            # On CI, skip using a persistent cache
            if (DEFINED ENV{CI})
                message(STATUS "Running in CI; skipping Cppcheck cache setup")
                set(CPPCHECK_ARGS ${CPPCHECK_BASE_ARGS})
            else()
                # Determine cache directory: prefer ~/.cache/cppcheck if HOME is set, else fallback
                if (DEFINED ENV{HOME})
                    file(REAL_PATH "~/.cache/cppcheck" CPPCHECK_CACHE_DIR EXPAND_TILDE)
                else()
                    set(CPPCHECK_CACHE_DIR "${CMAKE_BINARY_DIR}/.cppcheck-cache")
                endif()
                file(MAKE_DIRECTORY "${CPPCHECK_CACHE_DIR}")
                message(STATUS "Cppcheck cache directory: ${CPPCHECK_CACHE_DIR}")

                set(CPPCHECK_ARGS
                    ${CPPCHECK_BASE_ARGS}
                    "--cppcheck-build-dir=${CPPCHECK_CACHE_DIR}"
                )
            endif()

            # Configure cppcheck command lines
            set(CMAKE_C_CPPCHECK
                ${CPPCHECK_EXECUTABLE}
                ${CPPCHECK_ARGS}
            )
            set(CMAKE_CXX_CPPCHECK
                ${CPPCHECK_EXECUTABLE}
                ${CPPCHECK_ARGS}
            )

            message(STATUS "Cppcheck enabled: ${CPPCHECK_EXECUTABLE} (${CPPCHECK_ARGS})")
        else()
            message(WARNING "Cppcheck not found, skipping static analysis.")
        endif()
    endif()

	# IWYU integration
    if (RUN_IWYU)
        find_program(IWYU_EXECUTABLE include-what-you-use)
        if (IWYU_EXECUTABLE)
            set(IWYU_MAPPINGS_FILE "${XFLOW_UTILS_ROOT}/iwyu_mappings.imp")

            set(CMAKE_C_INCLUDE_WHAT_YOU_USE
                ${IWYU_EXECUTABLE}
                -Xiwyu --no_fwd_decls
                -Xiwyu --mapping_file=${IWYU_MAPPINGS_FILE}
            )
            set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${CMAKE_C_INCLUDE_WHAT_YOU_USE})

            message(STATUS "IWYU enabled: ${IWYU_EXECUTABLE}")
        else()
            message(WARNING "include-what-you-use not found, skipping IWYU integration.")
        endif()
    endif()


    # Flawfinder integration
    if (RUN_FLAWFINDER)
        find_program(FLAWFINDER_EXECUTABLE flawfinder)
        if (FLAWFINDER_EXECUTABLE)
			message(STATUS "FLAWFINDER_EXECUTABLE.")
            add_custom_target(flawfinder ALL
                COMMAND ${FLAWFINDER_EXECUTABLE} --context --columns --minlevel=1 ${PROJECT_SOURCE_DIR}
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                COMMENT "Running flawfinder on source code"
                VERBATIM
            )
        else()
            message(WARNING "Flawfinder not found, skipping flawfinder check.")
        endif()
    endif()
endif()

# clang-tidy integration
option(RUN_CLANG_TIDY_FIX "Apply clang-tidy fixes" OFF)

if (RUN_CLANG_TIDY)
	find_program(CLANG_TIDY_EXECUTABLE clang-tidy)
	if (CLANG_TIDY_EXECUTABLE)
		# Pick driver mode: llvm-mingw needs gcc; MSVC toolchain wants cl; others gcc.
		set(_TIDY_DRIVER "--extra-arg-before=--driver-mode=gcc")
		if (MSVC)
			set(_TIDY_DRIVER "--extra-arg-before=--driver-mode=cl")
		elseif (WIN32 AND CMAKE_C_COMPILER MATCHES "mingw|llvm-mingw")
			set(_TIDY_DRIVER "--extra-arg-before=--driver-mode=gcc")
		endif()
		set(_TIDY_COMMON_ARGS -p=${CMAKE_BINARY_DIR} ${_TIDY_DRIVER})
		if (DEFINED ENV{CI})
			list(APPEND _TIDY_COMMON_ARGS -warnings-as-errors=*)
		endif()
		if (NOT DEFINED CMAKE_C_CLANG_TIDY)
			if (RUN_CLANG_TIDY_FIX)
				set(CMAKE_C_CLANG_TIDY   "${CLANG_TIDY_EXECUTABLE};${_TIDY_COMMON_ARGS};-fix;-format-style=file")
				set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE};${_TIDY_COMMON_ARGS};-fix;-format-style=file")
			else()
				set(CMAKE_C_CLANG_TIDY   "${CLANG_TIDY_EXECUTABLE};${_TIDY_COMMON_ARGS}")
				set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE};${_TIDY_COMMON_ARGS}")
			endif()
			message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXECUTABLE} (args: ${_TIDY_COMMON_ARGS})")
		else()
			message(STATUS "clang-tidy already configured, skipping reconfiguration")
		endif()
	else()
		message(WARNING "clang-tidy not found, skipping clang-tidy analysis.")
	endif()
endif()

# scan-build (Clang Static Analyzer) integration
if (RUN_SCAN_BUILD)
	find_program(SCAN_BUILD_EXECUTABLE scan-build)
	if (SCAN_BUILD_EXECUTABLE)
		message(STATUS "SCAN_BUILD_EXECUTABLE.")
		# Only add the custom target if it doesn't exist yet
		if (NOT TARGET scan-build)
			add_custom_target(scan-build
				COMMAND ${SCAN_BUILD_EXECUTABLE}
						--status-bugs
						--use-analyzer=${CMAKE_C_COMPILER}
						cmake --build ${CMAKE_BINARY_DIR}
				WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
				COMMENT "Running scan-build (Clang Static Analyzer) on full build"
				VERBATIM
			)
			message(STATUS "scan-build target available: run `cmake --build . --target scan-build`")
		else()
			message(STATUS "scan-build target already exists, skipping duplicate definition")
		endif()
	else()
		message(WARNING "scan-build not found, skipping static analyzer build target.")
	endif()
endif()

message(STATUS "Building for system ${CMAKE_SYSTEM_NAME} with an ${CMAKE_SYSTEM_PROCESSOR} processor. CMAKE_CROSSCOMPILING is ${CMAKE_CROSSCOMPILING}.")

# Additional configuration
include(cmake/ipo.cmake)

option(SECTIONS_GC "Enable per-function/data sections and linker garbage collection" OFF)

if (SECTIONS_GC)
	include(CheckCCompilerFlag)
	include(CheckLinkerFlag)

	check_c_compiler_flag("-ffunction-sections" HAVE_FFUNCTION_SECTIONS)
	check_c_compiler_flag("-fdata-sections" HAVE_FDATA_SECTIONS)
	check_linker_flag(C "-Wl,--gc-sections" HAVE_LD_GC_SECTIONS)
	check_linker_flag(C "-Wl,-dead_strip" HAVE_LD_DEAD_STRIP)

	# Compile flags (non-Debug)
	if (HAVE_FFUNCTION_SECTIONS AND HAVE_FDATA_SECTIONS)
		add_compile_options($<$<NOT:$<CONFIG:Debug>>:-ffunction-sections>
		                    $<$<NOT:$<CONFIG:Debug>>:-fdata-sections>)
	endif()

	# Link flags (non-Debug), skip when ADDRESS_SANITIZER=ON
	if (APPLE AND HAVE_LD_DEAD_STRIP)
		add_link_options($<$<AND:$<NOT:$<CONFIG:Debug>>,$<NOT:$<BOOL:${ADDRESS_SANITIZER}>>>:-Wl,-dead_strip>)
	elseif (HAVE_LD_GC_SECTIONS)
		# Linux and Windows-MinGW hit this path
		add_link_options($<$<AND:$<NOT:$<CONFIG:Debug>>,$<NOT:$<BOOL:${ADDRESS_SANITIZER}>>>:-Wl,--gc-sections>)
	endif()
endif()

# Create the output directories
cmake_path(SET executable_dir ${PROJECT_BINARY_DIR}/${EXECUTABLES_DIR})
file(MAKE_DIRECTORY ${executable_dir})
cmake_path(APPEND executable_dir ${CONFIG_DIR} OUTPUT_VARIABLE final_config_dir)
file(MAKE_DIRECTORY ${final_config_dir})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${executable_dir})

set(CONFIGURE_WARNING "* ONLY EDIT THE .in VERSION OF THIS FILE!\n* THIS IS THE AUTOMATICALLY GENERATED FILE! DO NOT EDIT IT! YOUR CHANGES WILL BE OVERWRITTEN WHEN CMAKE RUNS AGAIN!")

set(CMAKE_INSTALL_SYSCONFDIR "config")
include(GNUInstallDirs)

add_subdirectory(include)
add_subdirectory(src)
if (MY_PROJECT_REALLY_IS_TOP_LEVEL AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	message(STATUS "MY_PROJECT_REALLY_IS_TOP_LEVEL and CMAKE_SOURCE_DIR ${CMAKE_SOURCE_DIR} = CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}")
	add_subdirectory(sim_example)
endif()
