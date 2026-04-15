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

find_package(jansson REQUIRED)
find_package(libmodbus REQUIRED)
find_package(GSL REQUIRED)
find_package(xflowutils REQUIRED)

# Threading support (portable pthread linking)
find_package(Threads REQUIRED)

# Platform-specific libraries for shared memory and dynamic loading
if(UNIX AND NOT APPLE)
    # Linux needs rt for shm_open/shm_unlink
    find_library(RT_LIBRARY rt)
    if(RT_LIBRARY)
        set(PLATFORM_LIBS ${RT_LIBRARY})
    endif()
endif()

# dl library (CMAKE_DL_LIBS is empty on platforms that don't need it)
list(APPEND PLATFORM_LIBS ${CMAKE_DL_LIBS})