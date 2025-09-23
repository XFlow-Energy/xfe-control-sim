# FindXflowutils.cmake
# -----------------------------------------------------------------------------
# © 2024–2025 XFlow Energy – https://www.xflowenergy.com/
#
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

# --- 0) Look for local zip first (unchanged) ---
set(_USE_LOCAL_ZIP FALSE)
set(_LOCAL_BASE "${CMAKE_SOURCE_DIR}/../xflow-utils/build/c/src/dist")
message(STATUS "xflow-utils local base dir: ${_LOCAL_BASE}")
if(EXISTS "${_LOCAL_BASE}")
	file(GLOB _LOCAL_MATCHES
		"${_LOCAL_BASE}/xflow-utils-*-${CMAKE_BUILD_TYPE}.zip"
		"${_LOCAL_BASE}/xflow-utils-*-Debug.zip"
		"${_LOCAL_BASE}/xflow-utils-*-Release.zip"
	)
endif()

# only keep the one matching our build type
if(_LOCAL_MATCHES)
	list(FILTER _LOCAL_MATCHES INCLUDE REGEX ".+-${CMAKE_BUILD_TYPE}\\.zip$")
	if(_LOCAL_MATCHES)
		list(GET _LOCAL_MATCHES 0 _LOCAL_ZIP)
		set(_USE_LOCAL_ZIP TRUE)
		set(XFLOW_UTILS_VERSION "local")
	endif()
endif()

# --- 1) If not local, resolve version (unchanged) ---
if(NOT _USE_LOCAL_ZIP)
	# --- Token resolution (CI env → local .secrets fallback) ---
	# Detect GitHub Actions
	set(_IN_CLOUD FALSE)
	if(DEFINED ENV{GITHUB_ACTIONS} AND "$ENV{GITHUB_ACTIONS}" STREQUAL "true")
		set(_IN_CLOUD TRUE)
	endif()

	# Priority:
	#  1) CMake cache/var XFLOW_UTILS_DIST_TOKEN if already set by caller
	#  2) ENV:XFLOW_UTILS_DIST_TOKEN (repo/Actions secret)
	#  3) ENV:GITHUB_TOKEN (default GitHub provided token in Actions)
	#  4) ../.secrets file (local dev)
	if(DEFINED XFLOW_UTILS_DIST_TOKEN AND NOT XFLOW_UTILS_DIST_TOKEN STREQUAL "")
		# keep as-is
	elseif(DEFINED ENV{XFLOW_UTILS_DIST_TOKEN} AND NOT "$ENV{XFLOW_UTILS_DIST_TOKEN}" STREQUAL "")
		set(XFLOW_UTILS_DIST_TOKEN "$ENV{XFLOW_UTILS_DIST_TOKEN}")
	elseif(_IN_CLOUD AND DEFINED ENV{GITHUB_TOKEN} AND NOT "$ENV{GITHUB_TOKEN}" STREQUAL "")
		set(XFLOW_UTILS_DIST_TOKEN "$ENV{GITHUB_TOKEN}")
	else()
		# Local dev fallback: read ../.secrets for XFLOW_UTILS_DIST_TOKEN
		set(_secrets_file "${CMAKE_SOURCE_DIR}/../.secrets")
		if(EXISTS "${_secrets_file}")
			file(STRINGS "${_secrets_file}" _secret_lines)
			foreach(_line IN LISTS _secret_lines)
				if(_line MATCHES "^[ \t]*XFLOW_UTILS_DIST_TOKEN[ \t]*=")
					string(REGEX REPLACE "^[^=]*=" "" XFLOW_UTILS_DIST_TOKEN "${_line}")
					string(STRIP "${XFLOW_UTILS_DIST_TOKEN}" XFLOW_UTILS_DIST_TOKEN)
				endif()
			endforeach()
		endif()
	endif()

	# --- Consolidated Pre-checks ---
	# Ensure the GitHub token is available for authentication.
	if(NOT DEFINED XFLOW_UTILS_DIST_TOKEN OR XFLOW_UTILS_DIST_TOKEN STREQUAL "")
		message(FATAL_ERROR "XFLOW_UTILS_DIST_TOKEN is not set; cannot authenticate to GitHub API.")
	endif()

	# Find the curl executable, failing if it's not found.
	find_program(CURL_EXECUTABLE curl REQUIRED)

	# --- Define API Call Parameters ---
	# Determine the specific API endpoint and local JSON file based on the requested version.
	if(XFLOW_UTILS_VERSION STREQUAL "" OR XFLOW_UTILS_VERSION STREQUAL "latest")
		set(_api_endpoint "latest")
		set(_api_json "${CMAKE_BINARY_DIR}/xflow-utils-release.json")
		set(_err_context "for latest release")
		set(_requested_is_tag FALSE)
	else()
		set(_api_endpoint "tags/${XFLOW_UTILS_VERSION}")
		set(_api_json "${CMAKE_BINARY_DIR}/xflow-utils-release-${XFLOW_UTILS_VERSION}.json")
		set(_err_context "for tag '${XFLOW_UTILS_VERSION}'")
		set(_requested_is_tag TRUE)
	endif()

	# --- Unified GitHub API Call ---
	# Execute a single curl command using the parameters defined above.
	execute_process(
		COMMAND "${CURL_EXECUTABLE}"
			-sS -L
			-H "Authorization: Bearer ${XFLOW_UTILS_DIST_TOKEN}"
			-H "User-Agent: xflow-utils-cmake"
			-H "Accept: application/vnd.github+json"
			-H "X-GitHub-Api-Version: 2022-11-28"
			"https://api.github.com/repos/XFlow-Energy/xflow-utils-dist/releases/${_api_endpoint}"
			-o "${_api_json}"
			-w "%{http_code}"
		RESULT_VARIABLE _curl_rv
		OUTPUT_VARIABLE _http_code
		ERROR_VARIABLE _curl_err
	)

	# --- Unified Error Handling ---
	# Check for curl errors or non-200 HTTP status codes.
	string(STRIP "${_http_code}" _http_code)
	if(NOT _curl_rv EQUAL 0 OR NOT _http_code MATCHES "^200$")
		if(_requested_is_tag AND _http_code STREQUAL "404")
			set(_error_message "GitHub API query failed ${_err_context}: tag not found (http=404).")
		else()
			set(_error_message "GitHub API query failed ${_err_context} (rv=${_curl_rv}, http=${_http_code}). curl error: ${_curl_err}")
		endif()
		if(EXISTS "${_api_json}")
			file(READ "${_api_json}" _err_body)
			string(APPEND _error_message "\nResponse: ${_err_body}")
		endif()
		message(FATAL_ERROR "${_error_message}")
	endif()

	# --- Unified JSON Parsing ---
	# Read the downloaded JSON and extract the tag_name to resolve the final version.
	file(READ "${_api_json}" _gh_content)

	if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.19")
		string(JSON resolved_tag GET "${_gh_content}" tag_name)
	else()
		string(REGEX MATCH "\"tag_name\"[ \t]*:[ \t]*\"([^\"]+)\"" _m "${_gh_content}")
		set(resolved_tag "${CMAKE_MATCH_1}")
	endif()

	if(NOT resolved_tag)
		message(FATAL_ERROR "Failed to parse tag_name from GitHub API response ${_err_context}.")
	endif()

	# If a specific tag was requested, verify the API returned the same tag.
	if(_requested_is_tag)
		if(NOT resolved_tag STREQUAL "${XFLOW_UTILS_VERSION}")
			message(FATAL_ERROR "Requested tag '${XFLOW_UTILS_VERSION}' but API returned '${resolved_tag}'.")
		endif()
	endif()

	# Set the final, resolved version.
	set(XFLOW_UTILS_VERSION "${resolved_tag}")

	message(STATUS "Resolved xflow-utils version to: ${XFLOW_UTILS_VERSION}")
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
			message(FATAL_ERROR "Failed to download xflow-utils: ${_dl_status}")
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
