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

# include/CMakeLists.txt

# Create an interface library for headers
if(NOT TARGET xfe-control-sim-include)
	add_library(xfe-control-sim-include INTERFACE)
endif()

target_sources(xfe-control-sim-include INTERFACE
	FILE_SET HEADERS
		BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
		FILES
			flow_gen.h
			numerical_integrator.h
			control_switch.h
			make_stage.h
			xfe_control_sim_version.h
)

# Add include directories
target_include_directories(xfe-control-sim-include INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR} # For generated xfe_control_sim_version.h
)

# # Conditionally add the customer models directory
# if(INTEGRATE_CUSTOMER_MODELS)
#     target_include_directories(xfe-control-sim-include INTERFACE ${xfe_control_sim_customer_dir})
# endif()

# pull in the modbus include-dirs:
target_link_libraries(xfe-control-sim-include INTERFACE xflow-utils-include)
