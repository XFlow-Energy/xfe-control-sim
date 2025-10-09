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
# Finder for prebuilt xflow-utils artifacts.
# Exposes:
#	- Targets (created if present in the distribution):
#		xflow-utils-include, xflow-utils, xflow-utils-shared, xflow-utils-static,
#		xflow-json-lib, xflow-modbus-lib, xflow-core-lib, xflow-math-lib,
#		xflow-shmem-sem-lib, xflow-aero-sim-lib, xflow-modbus-server-client-lib
#	- Vars:
#		Xflowutils_FOUND (TRUE/FALSE)
#
# Behavior mirrors original dependencies.cmake logic:
#	- Prefer a local zip in: ${XFE_CONTROL_SIM_PROJECT_DIR}/../xflow-utils/build/c/src/dist
#	- Otherwise download from GitHub releases (requires XFLOW_UTILS_DIST_TOKEN)
#	- Supports XFLOW_UTILS_VERSION cache var ("" or "latest" = query GitHub)
#	- Sets RPATH to only relative paths on non-Windows
#	- Imports shared/static/optional libs and a header-only interface
# -----------------------------------------------------------------------------

if(TARGET xflow-utils OR TARGET xflow-utils-shared OR TARGET xflow-utils-static)
	set(Xflowutils_FOUND TRUE)
	return()
endif()

# --- Inputs / knobs (kept as in original) ---
option(XFLOW_UTILS_USE_STATIC "Link xflow-utils statically if static archive is present" OFF)

# Project root context var used to search for local zip (unchanged)
# Expected to be set by the including project; if not set, the local-zip step is skipped gracefully.
#	set(XFE_CONTROL_SIM_PROJECT_DIR "/path/to/project")  # from parent project

# Find a local xflow-utils checkout by searching parent directories.
# This avoids hardcoding relative paths like ../xflow-utils.

if(NOT DEFINED XFLOW_UTILS_MAX_PARENT_DEPTH)
	set(XFLOW_UTILS_MAX_PARENT_DEPTH 8) # Set to -1 for no depth limit
endif()

# Discover the xflow-utils directory if a path isn't already supplied.
if(NOT DEFINED LOCAL_XFLOW_UTILS_DIR)
	# Set up search roots: start with the current directory, then the project root.
	set(_xflow_utils_roots "${CMAKE_CURRENT_SOURCE_DIR}")
	if (NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
		list(APPEND _xflow_utils_roots "${CMAKE_SOURCE_DIR}")
	endif()

	set(_xflow_utils_found FALSE)

	foreach(_root IN LISTS _xflow_utils_roots)
		if (_xflow_utils_found)
			break()
		endif()

		set(_dir "${_root}")
		set(_depth 0)
		while(TRUE)
			# At each level, check for a subdirectory named "xflow-utils"
			set(_candidate "${_dir}/xflow-utils")

			# A good check is for a sentinel file, like CMakeLists.txt, to verify it's the correct directory.
			if (EXISTS "${_candidate}/CMakeLists.txt")
				set(LOCAL_XFLOW_UTILS_DIR "${_candidate}")
				set(_xflow_utils_found TRUE)
				message(STATUS "Found local xflow-utils at ${LOCAL_XFLOW_UTILS_DIR}")
				break()
			endif()

			# Ascend to the parent directory for the next iteration.
			get_filename_component(_parent "${_dir}" DIRECTORY)
			if (_parent STREQUAL _dir)
				# We've reached the filesystem root, so stop.
				break()
			endif()

			# Enforce the maximum search depth.
			math(EXPR _depth "${_depth} + 1")
			if (NOT XFLOW_UTILS_MAX_PARENT_DEPTH EQUAL -1 AND _depth GREATER_EQUAL XFLOW_UTILS_MAX_PARENT_DEPTH)
				message(STATUS "Reached XFLOW_UTILS_MAX_PARENT_DEPTH=${XFLOW_UTILS_MAX_PARENT_DEPTH} at ${_dir}; stopping ascent.")
				break()
			endif()

			set(_dir "${_parent}")
		endwhile()
	endforeach()

	# If the search was performed but yielded no result, show an advisory message.
	if (NOT _xflow_utils_found)
		message(STATUS
			"Could not find a local xflow-utils directory by walking parents of:\n"
			"  ${CMAKE_CURRENT_SOURCE_DIR}\n"
			"  ${CMAKE_SOURCE_DIR}\n"
			"Checked each parent for a subdirectory 'xflow-utils' containing 'CMakeLists.txt'.\n"
			"Set -DLOCAL_XFLOW_UTILS_DIR=... to point at your checkout."
		)
	endif()
endif()

# --- Now, find the specific zip package using the located directory ---

set(_USE_LOCAL_ZIP FALSE)

# Proceed only if LOCAL_XFLOW_UTILS_DIR was found or provided by the user.
if(DEFINED LOCAL_XFLOW_UTILS_DIR AND EXISTS "${LOCAL_XFLOW_UTILS_DIR}")
    set(_LOCAL_BASE "${LOCAL_XFLOW_UTILS_DIR}/build/c/src/dist")
    message(STATUS "Checking for xflow-utils zip in: ${_LOCAL_BASE}")

    if(EXISTS "${_LOCAL_BASE}")
        # Find any zip files that could potentially match
        file(GLOB _LOCAL_MATCHES
            "${_LOCAL_BASE}/xflow-utils-*-${CMAKE_BUILD_TYPE}.zip"
            "${_LOCAL_BASE}/xflow-utils-*-Debug.zip"
            "${_LOCAL_BASE}/xflow-utils-*-Release.zip"
        )
    endif()

    # From the potential matches, filter to keep only the one for the current build type.
    if(_LOCAL_MATCHES)
        list(FILTER _LOCAL_MATCHES INCLUDE REGEX ".+-${CMAKE_BUILD_TYPE}\\.zip$")
        if(_LOCAL_MATCHES)
            list(GET _LOCAL_MATCHES 0 _LOCAL_ZIP)
            set(_USE_LOCAL_ZIP TRUE)
            set(XFLOW_UTILS_VERSION "local")
            message(STATUS "Using local xflow-utils zip: ${_LOCAL_ZIP}")
        endif()
    endif()
endif()

# --- 1) If not local, resolve version (restore 'latest' resolution) ---
if(NOT _USE_LOCAL_ZIP)
	if(XFLOW_UTILS_VERSION STREQUAL "" OR XFLOW_UTILS_VERSION STREQUAL "latest")
		find_program(CURL_EXECUTABLE curl)
		if(NOT CURL_EXECUTABLE)
			message(FATAL_ERROR "curl not found; required to fetch latest xflow-utils release info.")
		endif()

		# Optional token; if provided, we’ll send it, otherwise rely on unauthenticated (rate-limited) API.
		#	set(XFLOW_UTILS_DIST_TOKEN "ghp_xxx")  # optional

		set(_api_json "${CMAKE_BINARY_DIR}/xflow-utils-release.json")
		set(_curl_cmd "${CURL_EXECUTABLE}")
		set(_curl_args -sS -L
			-H "User-Agent: xflow-utils-cmake"
			-H "Accept: application/vnd.github+json"
			-H "X-GitHub-Api-Version: 2022-11-28"
		)
		if(DEFINED XFLOW_UTILS_DIST_TOKEN AND NOT XFLOW_UTILS_DIST_TOKEN STREQUAL "")
			list(APPEND _curl_args -H "Authorization: Bearer ${XFLOW_UTILS_DIST_TOKEN}")
		endif()
		list(APPEND _curl_args "https://api.github.com/repos/XFlow-Energy/xflow-utils-dist/releases/latest" -o "${_api_json}" -w "%{http_code}")

		execute_process(
			COMMAND "${_curl_cmd}" ${_curl_args}
			RESULT_VARIABLE _curl_rv
			OUTPUT_VARIABLE _http_code
			ERROR_VARIABLE _curl_err
		)
		string(STRIP "${_http_code}" _http_code)
		if(NOT _curl_rv EQUAL 0 OR NOT _http_code MATCHES "^200$")
			if(EXISTS "${_api_json}")
				file(READ "${_api_json}" _err_body)
				message(FATAL_ERROR "GitHub API query failed (rv=${_curl_rv}, http=${_http_code}). curl error: ${_curl_err}\nResponse: ${_err_body}")
			else()
				message(FATAL_ERROR "GitHub API query failed (rv=${_curl_rv}, http=${_http_code}). curl error: ${_curl_err}")
			endif()
		endif()

		file(READ "${_api_json}" _gh_content)
		if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.19")
			string(JSON XFLOW_UTILS_VERSION GET "${_gh_content}" tag_name)
			if(NOT XFLOW_UTILS_VERSION)
				message(FATAL_ERROR "Failed to parse tag_name from GitHub API")
			endif()
		else()
			string(REGEX MATCH "\"tag_name\"[ \t]*:[ \t]*\"([^\"]+)\"" _m "${_gh_content}")
			if(NOT CMAKE_MATCH_1)
				message(FATAL_ERROR "Failed to parse tag_name from GitHub API")
			endif()
			set(XFLOW_UTILS_VERSION "${CMAKE_MATCH_1}")
		endif()
		message(STATUS "xflow-utils: resolved latest tag '${XFLOW_UTILS_VERSION}'")
	else()
		message(STATUS "xflow-utils: using tag '${XFLOW_UTILS_VERSION}'")
	endif()
endif()

# --- 2) Platform / filenames (unchanged) ---
if(WIN32)
	set(_PLAT "windows")
	set(_ZIP  "xflow-utils-windows-${XFLOW_UTILS_VERSION}-${CMAKE_BUILD_TYPE}.zip")
	set(_SHLIB "libxflow-utils.dll")
	set(_IMPL  "libxflow-utils.dll.a")
elseif(APPLE)
	set(_PLAT "macos")
	set(_ZIP  "xflow-utils-macos-${XFLOW_UTILS_VERSION}-${CMAKE_BUILD_TYPE}.zip")
	set(_SHLIB "libxflow-utils.dylib")
elseif(UNIX)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
		set(_ARCH "arm64")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7l|arm")
		set(_ARCH "arm32")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
		set(_ARCH "amd64")
	else()
		message(FATAL_ERROR "Unsupported CPU: ${CMAKE_SYSTEM_PROCESSOR}")
	endif()
	set(_PLAT "linux-${_ARCH}")
	set(_ZIP  "xflow-utils-linux-${_ARCH}-${XFLOW_UTILS_VERSION}-${CMAKE_BUILD_TYPE}.zip")
	set(_SHLIB "libxflow-utils.so")
else()
	message(FATAL_ERROR "Unsupported platform for xflow-utils")
endif()

# --- 2.5) Relative RPATH (unchanged) ---
if(NOT WIN32)
	if(APPLE)
		set(_RPATH_PREFIX "@loader_path")
	else()
		set(_RPATH_PREFIX "\$ORIGIN")
	endif()
	set(CMAKE_BUILD_RPATH
		"${_RPATH_PREFIX}/../xflow-utils-${_PLAT}"
		"${_RPATH_PREFIX}/../lib"
	)
	set(CMAKE_INSTALL_RPATH
		"${_RPATH_PREFIX}/../xflow-utils-${_PLAT}"
		"${_RPATH_PREFIX}/../lib"
	)
	set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

set(_DEST "${CMAKE_BINARY_DIR}/xflow-utils-${_PLAT}")

# --- 3) Resolve the zip path (unchanged) ---
set(_SKIP_DOWNLOAD FALSE)
if(_USE_LOCAL_ZIP)
	get_filename_component(_REAL_ZIP_NAME "${_LOCAL_ZIP}" NAME)
	if(NOT EXISTS "${_DEST}")
		file(MAKE_DIRECTORY "${_DEST}")
	endif()
	file(COPY "${_LOCAL_ZIP}" DESTINATION "${_DEST}")
	set(_ZIPPATH "${_DEST}/${_REAL_ZIP_NAME}")
	set(_SKIP_DOWNLOAD TRUE)
else()
	set(_URL "https://github.com/XFlow-Energy/xflow-utils-dist/releases/download/${XFLOW_UTILS_VERSION}/${_ZIP}")
	set(_ZIPPATH "${_DEST}/${_ZIP}")
endif()

# refine to exact build type
list(FILTER _LOCAL_MATCHES INCLUDE REGEX ".+-${CMAKE_BUILD_TYPE}\\.zip$")
if (_LOCAL_MATCHES)
    list(LENGTH _LOCAL_MATCHES _num_local)
    if (_num_local GREATER 1)
        message(WARNING "Multiple local xflow-utils zips found; picking first: ${_LOCAL_MATCHES}")
    endif()
    list(GET _LOCAL_MATCHES 0 _LOCAL_ZIP)
    message(STATUS "Found local xflow-utils zip: ${_LOCAL_ZIP}")
    # copy into our _DEST location
    if (NOT EXISTS "${_DEST}")
        file(MAKE_DIRECTORY "${_DEST}")
    endif()
    file(COPY "${_LOCAL_ZIP}" DESTINATION "${_DEST}")
    # override ZIPPATH so we skip download
    set(_ZIPPATH "${_DEST}/${_REAL_ZIP_NAME}")
    # mark so download block is skipped
    set(_SKIP_DOWNLOAD TRUE)
endif()

# --- 4) Download/Extract (unchanged) ---
if(NOT _SKIP_DOWNLOAD)
	if(NOT EXISTS "${_DEST}/.stamp")
		file(MAKE_DIRECTORY "${_DEST}")
		message(STATUS "Downloading xflow-utils → ${_URL}")
		file(DOWNLOAD "${_URL}" "${_ZIPPATH}" SHOW_PROGRESS STATUS _dl_status)
		list(GET _dl_status 0 _dl_code)
		if(NOT _dl_code EQUAL 0)
			message(FATAL_ERROR "Failed to download xflow-utils: ${_dl_status}, url: ${_URL}")
		endif()
		message(STATUS "Extracting xflow-utils → ${_DEST}")
		file(ARCHIVE_EXTRACT INPUT "${_ZIPPATH}" DESTINATION "${_DEST}")
		if(APPLE)
			file(CREATE_LINK "${_DEST}/${_SHLIB}" "${_DEST}/libxflow-utils.1.dylib" SYMBOLIC)
		endif()
		if(UNIX AND NOT APPLE)
			file(CREATE_LINK "${_DEST}/${_SHLIB}" "${_DEST}/libxflow-utils.so.1" SYMBOLIC)
		endif()
		file(WRITE "${_DEST}/.stamp" "")
	endif()
else()
	if(NOT EXISTS "${_DEST}/.stamp")
		message(STATUS "Extracting local xflow-utils → ${_DEST}")
		file(ARCHIVE_EXTRACT INPUT "${_ZIPPATH}" DESTINATION "${_DEST}")
		if(APPLE)
			file(CREATE_LINK "${_DEST}/${_SHLIB}" "${_DEST}/libxflow-utils.1.dylib" SYMBOLIC)
		endif()
		if(UNIX AND NOT APPLE)
			file(CREATE_LINK "${_DEST}/${_SHLIB}" "${_DEST}/libxflow-utils.so.1" SYMBOLIC)
		endif()
		file(WRITE "${_DEST}/.stamp" "")
	endif()
endif()

# --- 5) Import shared library (unchanged) ---
if(NOT TARGET xflow-utils-shared)
	add_library(xflow-utils-shared SHARED IMPORTED GLOBAL)
	set_target_properties(xflow-utils-shared PROPERTIES
		IMPORTED_LOCATION "${_DEST}/${_SHLIB}"
		INTERFACE_INCLUDE_DIRECTORIES "${_DEST}/include"
	)
	if(WIN32)
		set_property(TARGET xflow-utils-shared PROPERTY IMPORTED_IMPLIB "${_DEST}/${_IMPL}")
	endif()
endif()

# --- 6) Import optional static sub-libs (unchanged) ---
foreach(_lib IN ITEMS
	xflow-json-lib
	xflow-modbus-lib
	xflow-core-lib
	xflow-math-lib
	xflow-shmem-sem-lib
	xflow-aero-sim-lib
	xflow-modbus-server-client-lib
)
	set(_libfile "${_DEST}/lib${_lib}.a")
	if(EXISTS "${_libfile}" AND NOT TARGET ${_lib})
		add_library(${_lib} STATIC IMPORTED GLOBAL)
		set_target_properties(${_lib} PROPERTIES
			IMPORTED_LOCATION "${_libfile}"
			INTERFACE_INCLUDE_DIRECTORIES "${_DEST}/include"
		)
	endif()
endforeach()

# 6b) If individual static sub-libs are present, attach their natural deps.
#     Most important: xflow-math-lib → GSL.
if(TARGET xflow-math-lib)
	# Only add if the consumer already found these packages (as in your dependencies.cmake)
	if(TARGET GSL::gslcblas)
		target_link_libraries(xflow-math-lib INTERFACE GSL::gslcblas)
	endif()
	if(TARGET GSL::gsl)
		target_link_libraries(xflow-math-lib INTERFACE GSL::gsl)
	endif()
endif()

# modbus sub-lib needs winsock on Windows
if(TARGET xflow-modbus-lib)
	if(WIN32)
		target_link_libraries(xflow-modbus-lib INTERFACE ws2_32)
	endif()
endif()


# --- 7) Import static “uber-archive” if shipped (unchanged) ---
set(_static_file "${_DEST}/libxflow-utils.a")
if(EXISTS "${_static_file}" AND NOT TARGET xflow-utils-static)
	add_library(xflow-utils-static STATIC IMPORTED GLOBAL)
	set_target_properties(xflow-utils-static PROPERTIES
		IMPORTED_LOCATION "${_static_file}"
		INTERFACE_INCLUDE_DIRECTORIES "${_DEST}/include"
	)
endif()

if(TARGET xflow-utils-static)
	# Headers already exported via INTERFACE_INCLUDE_DIRECTORIES above; now add link deps.
	# These are attached as INTERFACE so any consumer of xflow-utils-static gets them.
	if(TARGET jansson::jansson)
		target_link_libraries(xflow-utils-static INTERFACE jansson::jansson)
	endif()
	if(TARGET libmodbus::modbus)
		target_link_libraries(xflow-utils-static INTERFACE libmodbus::modbus)
	endif()
	if(TARGET GSL::gslcblas)
		target_link_libraries(xflow-utils-static INTERFACE GSL::gslcblas)
	endif()
	if(TARGET GSL::gsl)
		target_link_libraries(xflow-utils-static INTERFACE GSL::gsl)
	endif()
	if(WIN32)
		target_link_libraries(xflow-utils-static INTERFACE ws2_32)
	else()
		target_link_libraries(xflow-utils-static INTERFACE m)
	endif()
endif()

# --- 8) Public interface + header set (unchanged) ---
if(NOT TARGET xflow-utils-include)
	add_library(xflow-utils-include INTERFACE)
endif()
target_include_directories(xflow-utils-include INTERFACE "${_DEST}/include")

file(GLOB_RECURSE _XFLOW_UTILS_HEADERS "${_DEST}/include/*.h")
target_sources(xflow-utils-include PUBLIC
	FILE_SET HEADERS TYPE HEADERS
		BASE_DIRS "${_DEST}/include"
		FILES ${_XFLOW_UTILS_HEADERS}
)

if(NOT TARGET xflow-utils)
	add_library(xflow-utils INTERFACE)
endif()
target_link_libraries(xflow-utils INTERFACE xflow-utils-include)

# --- 9) Select shared vs static (unchanged) ---
if(XFLOW_UTILS_USE_STATIC)
	if(TARGET xflow-utils-static)
		set_target_properties(xflow-utils-static PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
		target_link_libraries(xflow-utils INTERFACE xflow-utils-static)
	else()
		message(FATAL_ERROR "Static xflow-utils requested but not found at ${_static_file}")
	endif()
else()
	if(TARGET xflow-utils-shared)
		set_target_properties(xflow-utils-shared PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
	endif()
	target_link_libraries(xflow-utils INTERFACE xflow-utils-shared)
endif()

# Link extras only for static builds (unchanged)
if(XFLOW_UTILS_USE_STATIC)
	if(WIN32)
		target_link_libraries(xflow-utils INTERFACE ws2_32)
	else()
		target_link_libraries(xflow-utils INTERFACE m)
	endif()
endif()

if(INTEGRATE_CUSTOMER_MODELS)
    if(NOT DEFINED CUSTOMER_MODEL_DIRECTORY OR NOT EXISTS "${CUSTOMER_MODEL_DIRECTORY}")
        message(FATAL_ERROR "INTEGRATE_CUSTOMER_MODELS is ON, but CUSTOMER_MODEL_DIRECTORY is not set or does not exist.")
    endif()

    message(STATUS "Found local customer xfe-control-sim at ${CUSTOMER_MODEL_DIRECTORY}")

    if(NOT TARGET aero-control-lib)
        add_subdirectory(
            "${CUSTOMER_MODEL_DIRECTORY}"
            "${CMAKE_BINARY_DIR}/${CUSTOMER_NAME}-local"
        )
    else()
        message(STATUS "aero-control-lib already added, skipping")
    endif()
endif()

set(Xflowutils_FOUND TRUE)
