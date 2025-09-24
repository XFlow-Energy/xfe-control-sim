/**
 * @file    drivetrain.c
 * @author  XFlow Energy
 * @date    2025
 * @brief   Drivetrain for testing software
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

#include "xfe_control_sim_common.h" // for param_array_t, read_csv_and_store
#include "drivetrains.h"            // for drivetrain
#include "logger.h"                 // for log_message
#include "make_stage.h"
#include "xflow_core.h"

#include <stdbool.h> // IWYU pragma: keep
#include <stddef.h>  // for NULL

// expand definitions once, using both the decl‐list and the call‐list
MAKE_STAGE_DEFINE(drivetrain, void, (DRIVETRAIN_PARAM_LIST), (DRIVETRAIN_CALL_ARGS))

void example_drivetrain(DRIVETRAIN_PARAM_LIST)
{
	static double *vfd_Torque_Command = NULL;
	static double *tau_Flow_Extract = NULL;
	static double *omega = NULL;
	static double *drivetrain_Drag = NULL;
	static int *enable_Brake_Signal = NULL;

	static bool first_Run = false;
	if (!first_Run)
	{
		// initialize variables since this is the first time the function is running.
		get_param(dynamic_data, "vfd_torque_command", &vfd_Torque_Command);
		get_param(dynamic_data, "tau_flow_extract", &tau_Flow_Extract);
		get_param(dynamic_data, "omega", &omega);
		get_param(dynamic_data, "drivetrain_drag", &drivetrain_Drag);
		get_param(dynamic_data, "enable_brake_signal", &enable_Brake_Signal);

		// log_message("vfd_torque_command before: %f\n", *vfd_Torque_Command);
		// log_message("tau_Flow_Extract before: %f\n", *tau_Flow_Extract);

		first_Run = true;
	}

	// log_message("vfd_torque_command before: %f\n", *vfd_Torque_Command);

	if (*enable_Brake_Signal != 0)
	{
		// *drivetrain_Drag = 450;
		// log_message("BRAKING! %f\n", *drivetrain_Drag);
	}
	else
	{
		*drivetrain_Drag = 0;
	}
}
