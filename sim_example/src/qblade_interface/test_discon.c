/**
 * @file    test_discon.c
 * @author  XFlow Energy
 * @date    2025
 * @brief   Testing qblade_interface
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

#include <stdbool.h>                // IWYU pragma: keep
#include <stddef.h>                 // for NULL
#include "xflow_control_sim_common.h" // for get_param, param_array_t
#include "xflow_core.h"
#include "logger.h" // for log_message
#include "qblade_interface.h"
#include "bladed_interface.h"
#include "drivetrains.h"      // for drivetrain
#include "turbine_controls.h" // for turbine_control
#include "discon.h"           // declare void DISCON(...)

int main(void)
{
	float avr_swap[REC_USER_VARIABLE_10] = {0};
	int avi_fail = -1;
	char acc_in_file[1] = {0};
	char avc_outname[1] = {0};
	char avc_msg[1] = {0};
	double simulation_time = 10.0;
	double elapsed_time = 0.0;
	avr_swap[REC_COMMUNICATION_INTERVAL] = 0.1;

	while (elapsed_time < simulation_time)
	{
		DISCON(DISCON_CALL_ARGS);
		if (avi_fail != 0)
		{
			return avi_fail;
		}
		elapsed_time += avr_swap[REC_COMMUNICATION_INTERVAL];
	}

	return EXIT_SUCCESS;
}
