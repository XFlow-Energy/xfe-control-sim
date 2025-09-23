# -----------------------------------------------------------------------------
# SPDX-License-Identifier: GPL-3.0-or-later
#
# XFE-CONTROL-SIM
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

# Do interprocedural optimization/link time optimization on release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT WIN32)
	include(CheckIPOSupported)
	check_ipo_supported(RESULT ipo_result)
	message(STATUS "Link Time Optimization support is ${ipo_result}.")
	if(ipo_result)
		message(STATUS "Enabling LTO for whole project.")
		# Globally enable LTO (adds -flto flags to compile and link)
		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

		# Determine platform and compiler to choose appropriate linker
		if(APPLE AND CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
			message(STATUS "macOS detected with AppleClang: using Apple ld64 for LTO")
		elseif((CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "GNU"))
			# On Linux & Windows use LLVM’s lld for LTO
			get_filename_component(_clang_bin "${CMAKE_C_COMPILER}" DIRECTORY)
			find_program(LLVM_LD NAMES lld lld-link HINTS "${_clang_bin}" "${_clang_bin}/..")
			if(LLVM_LD)
				message(STATUS "Using LLVM linker for LTO: ${LLVM_LD}")
				add_link_options(-fuse-ld=lld)
			endif()
		else()
			message(WARNING "Platform or compiler unsupported for custom LTO linker override; using default linker")
		endif()
	endif()
else()
	message(STATUS "LTO not enabled: build type is ${CMAKE_BUILD_TYPE}")
endif()
