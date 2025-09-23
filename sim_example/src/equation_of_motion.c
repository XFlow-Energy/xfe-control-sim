/**
 * @file    equation_of_motion.c
 * @author  XFlow Energy
 * @date    2025
 * @brief   Equation of Motion
 * the air. Used for testing simulation software
 */

/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * This file is part of the XFLOW-CONTROL-SIM example suite.
 *
 * To the extent possible under law, XFlow Energy has waived all copyright
 * and related or neighboring rights to this example file. This work is
 * published from: United States.
 *
 * You should have received a copy of the CC0 Public Domain Dedication along
 * with this software. If not, see <https://creativecommons.org/publicdomain/zero/1.0/>.
 */

#include "xflow_aero_sim.h"
#include "drivetrains.h"        // for drivetrain
#include "equation_of_motion.h" // for eom
#include "flow_sim_model.h"
#include "logger.h" // for log_message
#include "make_stage.h"
#include <stdbool.h> // IWYU pragma: keep
#include <stddef.h>  // for NULL
#include <string.h>  // for strcmp

// expand definitions once, using both the decl‐list and the call‐list
MAKE_STAGE_DEFINE(eom, void, (EOM_PARAM_LIST), (EOM_CALL_ARGS)) // NOLINT(readability-non-const-parameter)

void eom_simple_ball_thrown_in_air(EOM_PARAM_LIST)
{
	static double *dt_Sec = NULL;
	static double *gravity_Acc_G = NULL;
	static double *time_Sec = NULL;
	static int idx_Theta = -1;
	static int idx_Omega = -1;

	static bool first_Run = false;
	if (!first_Run)
	{
		// initialize variables since this is the first time the function is running.
		get_param(fixed_data, "dt_sec", &dt_Sec);
		get_param(fixed_data, "gravity_acc_g", &gravity_Acc_G);
		get_param(dynamic_data, "time_sec", &time_Sec);

		for (int i = 0; i < n_state_var; ++i)
		{
			if (strcmp(state_names[i], "theta") == 0)
			{
				idx_Theta = i;
			}
			else if (strcmp(state_names[i], "omega") == 0)
			{
				idx_Omega = i;
			}
		}

		// log_message("*state_vars[idx_Omega]: %f\n", *state_vars[idx_Omega]);
		// log_message("gravity_acc_g: %f\n", *gravity_Acc_G);
		// log_message("time_sec: %f\n", *time_Sec);

		first_Run = true;
	}

	// state_derivative[0] = state[1];
	// state_derivative[1] = -1.0 * (*gravity_Acc_G);
	// Directly assign to known indices (no name matching)
	dx[idx_Theta] = *state_vars[idx_Omega];
	dx[idx_Omega] = -1.0 * (*gravity_Acc_G);
}

void example_turbine_eom(EOM_PARAM_LIST)
{
	static double *moment_Of_Inertia = NULL;
	static int idx_Theta = -1;
	static int idx_Omega = -1;
	static double *drivetrain_Drag = NULL;
	static double *tau_Flow = NULL;
	static double *tau_Flow_Extract = NULL;

	static bool first_Run = false;
	if (!first_Run)
	{
		get_param(dynamic_data, "moment_of_inertia", &moment_Of_Inertia);
		get_param(dynamic_data, "tau_flow", &tau_Flow);
		get_param(dynamic_data, "tau_flow_extract", &tau_Flow_Extract);
		get_param(dynamic_data, "drivetrain_drag", &drivetrain_Drag);

		// Identify indices of state variables
		for (int i = 0; i < n_state_var; ++i)
		{
			if (strcmp(state_names[i], "theta") == 0)
			{
				idx_Theta = i;
			}
			else if (strcmp(state_names[i], "omega") == 0)
			{
				idx_Omega = i;
			}
		}

		if (idx_Theta < 0 || idx_Omega < 0)
		{
			ERROR_MESSAGE("eom(): required state variables not found\n");
			shutdownFlag = 1;
			return;
		}

		first_Run = true;
	}

	flow_sim_model(dynamic_data, fixed_data); // to get the updated tau_flow aero from the last timestep.

	// update the drivetrain stuff.
	drivetrain(dynamic_data, fixed_data); // to get the tau_Flow_Extract from the last timestep.

	// Directly assign to known indices (no name matching)
	dx[idx_Theta] = *state_vars[idx_Omega];                                                  // θ' = ω
	dx[idx_Omega] = (*tau_Flow - *tau_Flow_Extract - *drivetrain_Drag) / *moment_Of_Inertia; // ω' = (τ - T)/I
}
