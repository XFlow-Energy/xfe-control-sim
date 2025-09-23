/**
 * @file    control_switch.c
 * @author  XFlow Energy
 * @date    2025
 * @brief   Switch between different functions specified in the csv file
 * Used for testing the sim software and different control algorithm, kw^2
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

#include "xflow_control_sim_common.h" // for get_param, param_array_t
#include "control_switch.h"
#include "discon.h"
#include "drivetrains.h"
#include "logger.h" // for log_message
#include "make_stage.h"
#include "maybe_unused.h"
#include "qblade_interface.h"
#include "turbine_controls.h" // for turbine_control
#include <stdbool.h>          // IWYU pragma: keep
#include <stddef.h>           // for NULL

DEFINE_STAGE_DISPATCHER(turbine_control, turbineControlMap)
DEFINE_STAGE_DISPATCHER(drivetrain, drivetrainMap)
DEFINE_STAGE_DISPATCHER(qblade_interface, qbladeInterfaceMap)
DEFINE_STAGE_DISPATCHER(DISCON, disconMap)

void control_switch(CONTROL_SWITCH_PARAM_LIST)
{
	static const char *turbine_Control_Function_Call = NULL;
	static const char *drivetrain_Function_Call = NULL;
	static const char *qblade_Interface_Function_Call = NULL;
	static const char *discon_Function_Call = NULL;

	static bool first_Run = false;
	if (!first_Run)
	{
		get_param(fixed_data, "turbine_control_function_call", &turbine_Control_Function_Call);
		get_param(fixed_data, "drivetrain_function_call", &drivetrain_Function_Call);
		get_param(fixed_data, "qblade_interface_function_call", &qblade_Interface_Function_Call);
		get_param(fixed_data, "discon_function_call", &discon_Function_Call);

		// log_message("flow_function_call: %s\n", flow_function_call);
		// log_message("discon_Function_Call: %s\n", discon_Function_Call);

		// this runs the generic loop & sets the callback, or errors:
		DISPATCH_STAGE_OR_ERROR(turbine_control, turbineControlMap, turbine_Control_Function_Call);
		DISPATCH_STAGE_OR_ERROR(drivetrain, drivetrainMap, drivetrain_Function_Call);
		DISPATCH_STAGE_OR_ERROR(qblade_interface, qbladeInterfaceMap, qblade_Interface_Function_Call);
		DISPATCH_STAGE_OR_ERROR(DISCON, disconMap, discon_Function_Call);

		first_Run = true;
	}
}
