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
#	- Prefer a local zip in: ${XFLOW_CONTROL_SIM_PROJECT_DIR}/../xflow-utils/build/c/src/dist
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
#	set(XFLOW_CONTROL_SIM_PROJECT_DIR "/path/to/project")  # from parent project

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

# --- 1) If not local, resolve version (no tokens, no API) ---
if(NOT _USE_LOCAL_ZIP)
    # Treat "" as "latest"
    if(XFLOW_UTILS_VERSION STREQUAL "" OR XFLOW_UTILS_VERSION STREQUAL "latest")
        set(_USE_LATEST TRUE)
        set(_RESOLVED_REF "latest")
        message(STATUS "xflow-utils: using latest release (no GitHub token needed)")
    else()
        set(_USE_LATEST FALSE)
        set(_RESOLVED_REF "${XFLOW_UTILS_VERSION}")
        message(STATUS "xflow-utils: using tag '${_RESOLVED_REF}' (no GitHub token needed)")
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
    if(_USE_LATEST)
        # Redirects to the asset of the latest release
        set(_URL "https://github.com/XFlow-Energy/xflow-utils-dist/releases/latest/download/${_ZIP}")
    else()
        # Direct asset link for a specific tag
        set(_URL "https://github.com/XFlow-Energy/xflow-utils-dist/releases/download/${_RESOLVED_REF}/${_ZIP}")
    endif()
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

    message(STATUS "Found local customer xflow-control-sim at ${CUSTOMER_MODEL_DIRECTORY}")

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
