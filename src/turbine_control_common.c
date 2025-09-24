/**
 * @file    turbine_control.c
 * @author  XFlow Energy
 * @date    2025
 * @brief   Turbine control
 * Used for testing the sim software and different control algorithm, kw^2
 */

/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * XFE-CONTROL-SIM
 * Copyright (C) 2024-2025 XFlow Energy (https://www.xflowenergy.com/)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY and FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "turbine_controls.h" // for TURBINE_CONTROL_PARAM_LIST, kw2_turbin...
#include "xflow_aero_sim.h"   // for get_param
#include <stdbool.h>          // IWYU pragma: keep
#include <stddef.h>           // for NULL

void kw2_turbine_control(TURBINE_CONTROL_PARAM_LIST)
{
	static double *omega = NULL;
	static double *tau_Flow_Extract = NULL;
	static double *k = NULL;

	static bool first_Run = false;
	if (!first_Run)
	{
		// initialize variables since this is the first time the function is running.
		get_param(dynamic_data, "omega", &omega);
		get_param(dynamic_data, "tau_flow_extract", &tau_Flow_Extract);
		get_param(dynamic_data, "k", &k);

		// log_message("omega before: %f\n", *omega);

		first_Run = true;
	}

	*tau_Flow_Extract = (*k) * (*omega) * (*omega);
}
