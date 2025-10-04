/**
 * @file    test_discon.c
 * @author  XFlow Energy
 * @date    2025
 * @brief   Testing qblade_interface
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

#include "bladed_interface.h"
#include "xfe_control_sim_common.h" // for get_param, param_array_t
#include "discon.h"                 // declare void DISCON(...)
#include "drivetrains.h"            // for drivetrain
#include "logger.h"                 // for log_message
#include "make_stage.h"
#include "qblade_interface.h"
#include "turbine_controls.h" // for turbine_control
#include "xflow_core.h"
#include <stdbool.h> // IWYU pragma: keep
#include <stddef.h>  // for NULL

int main(void)
{
	float avr_swap[REC_USER_VARIABLE_10] = {0};
	int avi_fail = -1;
	char acc_in_file[1] = {0};
	char avc_outname[1] = {0};
	char avc_msg[1] = {0};

	/* Simple plant + sim settings */
	double simulation_time = 10.0;
	double t = 0.0;
	double omega = 0.0;
	float dt = 0.1f;

	/* Controller I/O seeds */
	avr_swap[REC_COMMUNICATION_INTERVAL] = dt;
	avr_swap[REC_CURRENT_TIME] = (float)t;
	avr_swap[REC_MEASURED_ROTOR_SPEED] = (float)omega;

	/* Provide target speed and inertia to controller (read in your interface on first call) */
	avr_swap[REC_USER_VARIABLE_1] = 2.0f;  /* omega_target [rad/s], example */
	avr_swap[REC_USER_VARIABLE_2] = 50.0f; /* moment_of_inertia J [kg·m^2], example */

	while (t < simulation_time)
	{
		/* Present current measurements BEFORE calling the controller */
		avr_swap[REC_CURRENT_TIME] = (float)t;
		avr_swap[REC_MEASURED_ROTOR_SPEED] = (float)omega;

		DISCON(DISCON_CALL_ARGS);
		if (avi_fail != 0)
		{
			return avi_fail;
		}

		/* Plant integration: ω_{k+1} = ω_k + (τ_cmd/J)*dt */
		double tau_cmd = (double)avr_swap[REC_DEMANDED_GENERATOR_TORQUE];
		double J = (double)avr_swap[REC_USER_VARIABLE_2];
		if (J <= 0.0)
		{
			J = 1.0;
		}
		omega += (tau_cmd / J) * (double)dt;

		t += (double)dt;
	}

	return EXIT_SUCCESS;
}
