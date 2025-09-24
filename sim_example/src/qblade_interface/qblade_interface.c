/**
 * @file    qblade_interface_xflow.c
 * @author  XFlow Energy
 * @date    2025
 * @brief   QBlade interface for XFlow Energy specifically
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

#include "xfe_control_sim_common.h" // for get_param, param_array_t
#include "bladed_interface.h"
#include "drivetrains.h" // for drivetrain
#include "logger.h"      // for log_message
#include "make_stage.h"
#include "qblade_interface.h"
#include "turbine_controls.h" // for turbine_control
#include "xflow_core.h"
#include <stdbool.h> // IWYU pragma: keep
#include <stddef.h>  // for NULL

// expand definitions once, using both the decl‐list and the call‐list
MAKE_STAGE_DEFINE(qblade_interface, void, (QBLADE_INTERFACE_PARAM_LIST), (QBLADE_INTERFACE_CALL_ARGS)) // NOLINT(readability-non-const-parameter)

/**
 * @brief Executes the QBlade-based control algorithm for each DISCON call.
 *
 * This function implements the core runtime logic of the external controller
 * invoked by the DISCON interface. On first invocation, it binds the required
 * input parameters (`omega`, `tau_Flow_Extract`, `time_sec`, `dt_sec`,
 * `control_dt_sec`) from the dynamic and fixed parameter arrays via `get_param()`
 * and initializes the communication interval (`dt_sec`) from `avr_swap[REC_COMMUNICATION_INTERVAL]`.
 *
 * On each call thereafter:
 * 1. Updates the current simulation time (`REC_CURRENT_TIME`) and rotor speed
 *    (`REC_MEASURED_ROTOR_SPEED`) into the bound variables.
 * 2. Accumulates elapsed time and, when it exceeds `control_dt_sec`, invokes
 *    `turbine_control()` to compute the next generator torque command and
 *    resets the accumulator.
 * 3. Always calls `drivetrain()` to update the low-speed shaft torque demand
 *    (`tau_Flow_Extract`).
 * 4. Writes the demanded generator torque back into `avr_swap[REC_DEMANDED_GENERATOR_TORQUE]`.
 * 5. Performs continuous logging via `continuous_logging_function()`.
 *
 * @param[in,out] avr_swap       Array of floats passed by Bladed/DISCON containing
 *                              input signals and receiving the output torque command.
 * @param[in]     dynamic_data  Pointer to the array of dynamic (state) parameters.
 * @param[in]     fixed_data    Pointer to the array of fixed parameters.
 */
void example_qblade_interface(QBLADE_INTERFACE_PARAM_LIST)
{
	static double *omega = NULL;
	static double *tau_Flow_Extract = NULL;
	static double *time_Sec = NULL;
	static double *dt_Sec = NULL;
	static double *control_Dt_Sec = NULL;
	static double accumulated_Time = 0.0;

	static bool first_Run = false;

	if (!first_Run)
	{
		// initialize variables since this is the first time the function is running.
		get_param(dynamic_data, "omega", &omega);
		get_param(dynamic_data, "tau_flow_extract", &tau_Flow_Extract);
		get_param(dynamic_data, "time_sec", &time_Sec);
		get_param(fixed_data, "dt_sec", &dt_Sec);
		get_param(fixed_data, "control_dt_sec", &control_Dt_Sec);
		log_message("omega: %f\n", *omega);
		log_message("tau_Flow_Extract: %f\n", *tau_Flow_Extract);
		log_message("time_sec: %f\n", *time_Sec);

		// even though this is fixed data this can still change once to make sure qbalde interval matches...
		*dt_Sec = avr_swap[REC_COMMUNICATION_INTERVAL];

		first_Run = true;
	}
	// set current time and speed
	*time_Sec = avr_swap[REC_CURRENT_TIME];
	*omega = avr_swap[REC_MEASURED_ROTOR_SPEED];

	// Add the elapsed time since the last update
	accumulated_Time += *dt_Sec;

	// Check if the accumulated time has reached or exceeded control_dt_sec
	if (accumulated_Time >= *control_Dt_Sec)
	{
		// call the turbine control every control_dt_sec timestep.
		turbine_control(dynamic_data, fixed_data); // update the vfd torque command
		accumulated_Time -= *control_Dt_Sec;       // Reset accumulated_Time, preserve any leftover time
	}

	drivetrain(dynamic_data, fixed_data); // update the low speed torque desired, essentially tau_gen

	avr_swap[REC_DEMANDED_GENERATOR_TORQUE] = (float)(*tau_Flow_Extract);

	continuous_logging_function(dynamic_data, fixed_data);
}
