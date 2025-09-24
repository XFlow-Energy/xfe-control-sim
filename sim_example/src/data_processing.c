/**
 * @file    data_processing.c
 * @author  XFlow Energy
 * @date    2025
 * @brief   Placeholder to use when no data optimization is being called.
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

#ifdef __APPLE__
#include <mach/host_info.h>
#include <mach/mach.h>
#include <sys/types.h> // for pid_t, off_t
#include <unistd.h>
#endif
#ifdef _WIN32
// NOLINTBEGIN(llvm-include-order)
#include <winsock2.h>
#include <windows.h>
// NOLINTEND(llvm-include-order)
#endif
#include "data_processing.h" // for data_processing
#include "logger.h"          // for log_message
#include "make_stage.h"
#include "maybe_unused.h"
#include "xfe_control_sim_common.h" // for get_param, param_array_t
#include "xflow_aero_sim.h"
#include "xflow_core.h" // for usleep_now, shutdownFlag, get_real_time...
#include "xflow_math.h"
#include "xflow_shmem_sem.h"
#include <math.h>    // for pow, fabs
#include <stdbool.h> // IWYU pragma: keep
#include <stddef.h>  // for NULL
#include <stdio.h>
#include <stdlib.h> // for free, exit, EXIT_FAILURE, malloc
#include <string.h>

// expand definitions once, using both the decl‐list and the call‐list
MAKE_STAGE_DEFINE(data_processing, void, (DATA_PROCESSING_PARAM_LIST), (DATA_PROCESSING_CALL_ARGS))

void example_data_processing(DATA_PROCESSING_PARAM_LIST)
{
	// Suppress unused-parameter warnings: these parameters are required by the callback signature
	// but the default implementation does not actually use them.
	(void)dp_program_options;

	static bool first_Run = false;

	if (!first_Run)
	{

		// log_message("Turbine data initialized, data_processing, simulation_points: %f simulation_time: %f\n",simulation_points, simulation_time);

		first_Run = true;
	}
}
