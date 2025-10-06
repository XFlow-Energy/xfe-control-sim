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
	static double *time_Sec = NULL;
	static int *total_Loop_Count = NULL;
	// static char **all_Combined = NULL;

	static param_history_accessor_t omega_History_Accessor;
	static param_history_accessor_t total_Loop_Count_History_Accessor;
	// static param_history_accessor_t all_Combined_History_Accessor;
	static param_history_accessor_t time_Sec_History_Accessor;

	static bool first_Run = false;

	if (!first_Run)
	{
		get_param(dynamic_data, "tau_flow_extract", &tau_Flow_Extract);
		get_param(dynamic_data, "k", &k);

		// Allocate space for 10 historical omega values
		get_param_history(dynamic_data, "omega", &omega_History_Accessor);
		get_param_history(dynamic_data, "total_loop_count", &total_Loop_Count_History_Accessor);
		// get_param_history(dynamic_data, "all_combined", &all_Combined_History_Accessor);
		get_param_history(dynamic_data, "time_sec", &time_Sec_History_Accessor);

		// Now you can directly access as an array!
		omega = (double *)omega_History_Accessor.local_buffer;
		total_Loop_Count = (int *)total_Loop_Count_History_Accessor.local_buffer;
		// all_Combined = (char **)all_Combined_History_Accessor.local_buffer;
		time_Sec = (double *)time_Sec_History_Accessor.local_buffer;

		first_Run = true;
	}

	// Refresh the local buffer with latest values
	refresh_history_local_buffer(&omega_History_Accessor);
	refresh_history_local_buffer(&total_Loop_Count_History_Accessor);
	// refresh_history_local_buffer(&all_Combined_History_Accessor);
	refresh_history_local_buffer(&time_Sec_History_Accessor);

	int count = omega_History_Accessor.local_valid_count;

	log_message("Omega history has %d/%d values:\n", count, *omega_History_Accessor.size);
	for (int i = 0; i < count; i++)
	{
		// const char *combined_str = all_Combined[i] ? all_Combined[i] : "(null)";
		// log_message("time_Sec[%d]: %f, omega[%d] = %f, loop count[%d]: %d, all_combined: %s\n", i, time_Sec[i], i, omega[i], i, total_Loop_Count[i], combined_str);
		log_message("time_Sec[%d]: %f, omega[%d] = %f, loop count[%d]: %d\n", i, time_Sec[i], i, omega[i], i, total_Loop_Count[i]);
	}

	// Use most recent value (index 0)
	if (count > 0)
	{
		*tau_Flow_Extract = (*k) * omega[0] * omega[0];
	}
}
