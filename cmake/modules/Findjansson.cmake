# -----------------------------------------------------------------------------
# SPDX-License-Identifier: GPL-3.0-or-later
#
# XFLOW-CONTROL-SIM
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

# Findjansson.cmake
#
# Find the jansson JSON library
#
# This module defines:
#   JANSSON_FOUND - True if jansson is found
#   JANSSON_INCLUDE_DIRS - Include directories for jansson
#   JANSSON_LIBRARIES - Libraries to link against
#   JANSSON_VERSION - Version of jansson (if available)
#
# And creates the imported target:
#   jansson::jansson - The jansson library target

# Prevent multiple inclusions
if(TARGET jansson::jansson)
    return()
endif()

# Try to find jansson using pkg-config first (most reliable for version info)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_JANSSON QUIET jansson)
endif()

# Determine search paths based on build type
if(CMAKE_CROSSCOMPILING)
    # Cross-compiling search paths (e.g., BeagleBone)
    set(_jansson_include_hints 
        "${CMAKE_SYSROOT}/usr/include"
        "${CMAKE_SYSROOT}/usr/include/${CMAKE_LIBRARY_ARCHITECTURE}"
        ${PC_JANSSON_INCLUDE_DIRS}
    )
    set(_jansson_library_hints
        "${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
        "${CMAKE_SYSROOT}/usr/lib"
        ${PC_JANSSON_LIBRARY_DIRS}
    )
else()
    # Native build search paths
    set(_jansson_include_hints
        ${PC_JANSSON_INCLUDE_DIRS}
        # Windows paths
        "C:/PROGRA~1/jansson/include"
        # Standard system paths will be searched automatically
    )
    set(_jansson_library_hints
        ${PC_JANSSON_LIBRARY_DIRS}
        # Windows paths  
        "C:/PROGRA~1/jansson/lib"
        # Standard system paths will be searched automatically
    )
endif()

# Find the include directory
find_path(JANSSON_INCLUDE_DIR
    NAMES jansson.h
    HINTS ${_jansson_include_hints}
          "C:/PROGRA~1/jansson/include"
    DOC "Directory where jansson.h is located"
)

# Debug message
message(STATUS "JANSSON_INCLUDE_DIR: ${JANSSON_INCLUDE_DIR}")

# Find the library
find_library(JANSSON_LIBRARY
    NAMES jansson libjansson
    HINTS ${_jansson_library_hints}
          "C:/PROGRA~1/jansson/lib"
    DOC "jansson library"
)

# Debug message
message(STATUS "JANSSON_LIBRARY: ${JANSSON_LIBRARY}")

# Get version from pkg-config if available
if(PC_JANSSON_VERSION)
    set(JANSSON_VERSION "${PC_JANSSON_VERSION}")
elseif(JANSSON_INCLUDE_DIR AND EXISTS "${JANSSON_INCLUDE_DIR}/jansson.h")
    # Try to extract version from header if pkg-config didn't work
    file(STRINGS "${JANSSON_INCLUDE_DIR}/jansson.h" _jansson_version_line
         REGEX "^#define[ \t]+JANSSON_VERSION[ \t]+\"[^\"]*\"")
    if(_jansson_version_line)
        string(REGEX REPLACE "^#define[ \t]+JANSSON_VERSION[ \t]+\"([^\"]*)\""
               "\\1" JANSSON_VERSION "${_jansson_version_line}")
    endif()
endif()

# Use CMake's standard mechanism to handle the found/not found logic
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(jansson
    REQUIRED_VARS JANSSON_LIBRARY JANSSON_INCLUDE_DIR
    VERSION_VAR JANSSON_VERSION
)

# Create the imported target if found
if(JANSSON_FOUND)
    # Set legacy variables for compatibility
    set(JANSSON_LIBRARIES ${JANSSON_LIBRARY})
    set(JANSSON_INCLUDE_DIRS ${JANSSON_INCLUDE_DIR})
    
    # Create the modern imported target
    add_library(jansson::jansson INTERFACE IMPORTED)
    set_target_properties(jansson::jansson PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${JANSSON_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${JANSSON_LIBRARY}"
    )
    
    # Debug output
    if(NOT jansson_FIND_QUIETLY)
        message(STATUS "Found jansson: ${JANSSON_LIBRARY} (found version \"${JANSSON_VERSION}\")")
        message(STATUS "jansson include dir: ${JANSSON_INCLUDE_DIR}")
    endif()
endif()

# Clean up internal variables
unset(_jansson_include_hints)
unset(_jansson_library_hints)
unset(_jansson_version_line)