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

# Findlibmodbus.cmake
# Provides:
#	- Target: libmodbus::modbus
#	- Alias:  libmodbus::libmodbus
#	- Vars:   libmodbus_FOUND, libmodbus_INCLUDE_DIRS, libmodbus_LIBRARIES, LIBMODBUS_VERSION

cmake_minimum_required(VERSION 3.16)

set(LIBMODBUS_ROOT "" CACHE PATH "Root prefix where libmodbus is installed (e.g., C:/libmodbus or /opt/homebrew/opt/libmodbus)")

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
	pkg_check_modules(PC_LIBMODBUS QUIET libmodbus)
endif()

set(_HINT_INC)
set(_HINT_LIB)

# pkg-config hints
if(PC_LIBMODBUS_FOUND)
	list(APPEND _HINT_INC ${PC_LIBMODBUS_INCLUDEDIR} ${PC_LIBMODBUS_INCLUDE_DIRS})
	list(APPEND _HINT_LIB ${PC_LIBMODBUS_LIBDIR} ${PC_LIBMODBUS_LIBRARY_DIRS})
endif()

# User override
if(LIBMODBUS_ROOT)
	list(APPEND _HINT_INC "${LIBMODBUS_ROOT}/include")
	list(APPEND _HINT_LIB "${LIBMODBUS_ROOT}/lib" "${LIBMODBUS_ROOT}/lib64")
endif()

# Cross-compile sysroot
if(CMAKE_SYSROOT)
	list(APPEND _HINT_INC
		"${CMAKE_SYSROOT}/usr/include"
		"${CMAKE_SYSROOT}/usr/include/${CMAKE_LIBRARY_ARCHITECTURE}"
	)
	list(APPEND _HINT_LIB
		"${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
		"${CMAKE_SYSROOT}/usr/lib"
		"${CMAKE_SYSROOT}/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
		"${CMAKE_SYSROOT}/lib"
	)
endif()

# macOS: Homebrew + /usr/local + opt symlink
if(APPLE)
	list(APPEND _HINT_INC "/opt/homebrew/include" "/usr/local/include" "/opt/homebrew/opt/libmodbus/include")
	list(APPEND _HINT_LIB "/opt/homebrew/lib" "/usr/local/lib" "/opt/homebrew/opt/libmodbus/lib")
endif()

# Windows: common locations (avoid $ENV{ProgramFiles(x86)}; use 8.3 short paths)
if(WIN32)
	list(APPEND _HINT_INC
		"C:/libmodbus/include"
		"C:/PROGRA~1/libmodbus/include"	# Program Files
		"C:/PROGRA~2/libmodbus/include"	# Program Files (x86)
		"$ENV{ProgramFiles}/libmodbus/include"	# okay (no parentheses)
	)
	list(APPEND _HINT_LIB
		"C:/libmodbus/lib"
		"C:/PROGRA~1/libmodbus/lib"
		"C:/PROGRA~2/libmodbus/lib"
		"$ENV{ProgramFiles}/libmodbus/lib"
	)
endif()

# Locate headers and library
find_path(LIBMODBUS_INCLUDE_DIR
	NAMES modbus/modbus.h
	HINTS ${_HINT_INC}
)

find_library(LIBMODBUS_LIBRARY
	NAMES modbus libmodbus
	HINTS ${_HINT_LIB}
)

# Version via pkg-config (optional)
if(PC_LIBMODBUS_VERSION)
	set(LIBMODBUS_VERSION "${PC_LIBMODBUS_VERSION}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libmodbus
	FOUND_VAR libmodbus_FOUND
	REQUIRED_VARS LIBMODBUS_LIBRARY LIBMODBUS_INCLUDE_DIR
	VERSION_VAR LIBMODBUS_VERSION
)

if(libmodbus_FOUND)
	set(LIBMODBUS_INCLUDE_DIRS "${LIBMODBUS_INCLUDE_DIR}")
	set(LIBMODBUS_LIBRARIES "${LIBMODBUS_LIBRARY}")

	if(NOT TARGET libmodbus::modbus)
		add_library(libmodbus::modbus UNKNOWN IMPORTED)
		set_target_properties(libmodbus::modbus PROPERTIES
			IMPORTED_LOCATION "${LIBMODBUS_LIBRARY}"
			INTERFACE_INCLUDE_DIRECTORIES "${LIBMODBUS_INCLUDE_DIR}"
		)
	endif()

	# Compat alias some projects use
	if(NOT TARGET libmodbus::libmodbus)
		add_library(libmodbus::libmodbus INTERFACE IMPORTED)
		set_target_properties(libmodbus::libmodbus PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES "${libmodbus_INCLUDE_DIRS}"
			INTERFACE_LINK_LIBRARIES "${libmodbus_LIBRARIES}"
		)
		add_dependencies(libmodbus::libmodbus libmodbus::modbus)
	endif()

	message(STATUS "Found libmodbus: ${LIBMODBUS_LIBRARY} (include: ${LIBMODBUS_INCLUDE_DIR})")
endif()