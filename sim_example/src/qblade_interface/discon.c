/**
 * @file    discon.c
 * @author  XFlow Energy
 * @date    2025
 * @brief   Interface for QBlade
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

#include <stdio.h>
#include "xflow_control_sim_common.h" // for param_array_t, (anonymous struct)
#include "xflow_core.h"
#include "xflow_aero_sim.h"
#include "logger.h" // for log_message, ERROR_MESSAGE
#include "maybe_unused.h"
#include "qblade_interface.h" // for turbine_control
#include "control_switch.h"
#include "discon.h" // for turbine_control

#define NINT(a) ((a) >= 0.0 ? (int)((a) + 0.5) : (int)((a) - 0.5))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

MAKE_STAGE_DEFINE(DISCON, void, (DISCON_PARAM_LIST), (DISCON_DISPATCH_ARGS))

// needs to be here for initialization.
__attribute__((constructor)) static void init_discon_hook(void)
{
	register_DISCON(example_discon);
}

/**
 * @brief Entry point for the DISCON controller called by GH Bladed.
 *
 * This function implements the DISCON (DIgital Simulated CONtroller) interface:
 * an external control‐algorithm hook used by DNV GL’s Bladed wind turbine simulator.
 * Bladed invokes DISCON at each control step, passing in its array of averaged
 * signals (`avr_swap`). The controller reads key sensor values (via `NINT(avr_swap[0])`),
 * executes the QBlade control algorithm (`qblade_interface`) using both dynamic and
 * fixed parameters, and returns a status flag (`avi_fail`) to indicate success or failure.
 *
 * **DISCON and the Bladed interface:**
 * - **DISCON** is the standardized C API that allows Bladed to call an external wind
 *   turbine controller at runtime. It defines a set of input/output arrays and string
 *   arguments for message passing, file paths, and inter‐process communication.
 * - Bladed populates the `avr_swap` buffer with time‐averaged measurements (e.g., rotor
 *   speed, generator torque) before each control step, then calls `DISCON`.
 * - The controller writes any diagnostic or control messages into the supplied character
 *   buffers (`acc_in_file`, `avc_outname`, `avc_msg`), and sets `avi_fail = 0` on success.
 *
 * @param[in,out] avr_swap     Array of averaged input signals from Bladed (e.g., speeds, loads).
 * @param[out]    avi_fail     Output flag: set to 0 for success, nonzero for failure.
 * @param[in]     acc_in_file   (Unused) Input message or filename passed from Bladed.
 * @param[in]     avc_outname  (Unused) Path for controller output files under the simulation folder.
 * @param[in]     avc_msg      (Unused) Text message buffer to send status or error back to Bladed.
 */
void example_discon(DISCON_PARAM_LIST)
{
	static bool init_complete = false;
	static param_array_t *dynamic_data = NULL;
	static param_array_t *fixed_data = NULL;

	if (!init_complete)
	{
		int n_params = 1;
		dynamic_data = create_input_data(n_params);
		fixed_data = create_input_data(n_params);

		// Pass the address of the pointers (i.e., pointers to pointers)
		initialize_control_system(&dynamic_data, &fixed_data, 1);

		control_switch(dynamic_data, fixed_data);

		log_message("discon init complete!\n");

		init_complete = true;
	}

	// avr_swap - data swap array
	// avi_fail  - flag to tell caller if call is successful
	// acc_in_file - character array message passed into controller
	// avc_outname - char array to path to sim results folder, for saving controller data internally
	// avc_msg - char array, can be used to send message up to caller

	int i_status; //, iFirstLog;

	// Load variables from Bladed (See Appendix A)
	i_status = NINT(avr_swap[0]);

	if (i_status >= 0)
	{
		qblade_interface(avr_swap, dynamic_data, fixed_data);
	}

	// Indicate successful execution
	*avi_fail = 0;
}
