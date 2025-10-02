/**
 * @file    turbine_control.c
 * @author  XFlow Energy
 * @date    2025
 * @brief   Turbine control
 * Used for testing the sim software and different control algorithm, kw^2
 */

/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * This file is part of the XFE-CONTROL-SIM example suite.
 *
 * To the extent possible under law, XFlow Energy has waived all copyright
 * and related or neighboring rights to this example file. This work is
 * published from: United States.
 *
 * You should have received a copy of the CC0 Public Domain Dedication along
 * with this software. If not, see <https://creativecommons.org/publicdomain/zero/1.0/>.
 */

// NOLINTBEGIN(llvm-include-order)
#include "make_stage.h"
#include "xfe_control_sim_common.h" // for get_param, param_array_t
#include "logger.h"                 // for log_message
#include "maybe_unused.h"
#include "turbine_controls.h" // for turbine_control
#include <stdbool.h>          // IWYU pragma: keep
#include <stddef.h>           // for NULL
							  // NOLINTEND(llvm-include-order)

// expand definitions once, using both the decl‐list and the call‐list
MAKE_STAGE_DEFINE(turbine_control, void, (TURBINE_CONTROL_PARAM_LIST), (TURBINE_CONTROL_CALL_ARGS))

void example_turbine_control(TURBINE_CONTROL_PARAM_LIST)
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
