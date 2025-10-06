/**
 * @file    flow_gen.c
 * @author  XFlow Energy
 * @date    2025
 * @brief   Flow generation for flow speed from a .csv and .bts file
 */

/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * XFE-CONTROL-SIM
 * Copyright (C) 2024-2025 XFlow Energy (https://www.xflowenergy.com/)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY and FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "xflow_core.h"
#include "flow_gen.h"               // for flow
#include "xfe_control_sim_common.h" // for shutdownFlag, create_shared_interp
#include "xflow_aero_sim.h"
#include <stddef.h> // for NULL

// IWYU pragma: begin_exports
#include <stdarg.h> // for va_end, va_list, va_start
// IWYU pragma: end_exports
#include <math.h>
#include <stdbool.h> // IWYU pragma: keep
#include <stdlib.h>  // for exit, EXIT_FAILURE, NULL, size_t

#ifdef _WIN32
// NOLINTBEGIN(llvm-include-order)
#include <windows.h> // For MAX_PATH, CreateFileMapping, MapViewOfFile, etc.
#include <process.h> // for getpid
#include <direct.h>  // for _mkdir
#include <inttypes.h>
#include <io.h>
// NOLINTEND(llvm-include-order)
#else
#include <limits.h>   // for PATH_MAX
#include <string.h>   // for strlen, strcmp
#include <sys/mman.h> // for shm_unlink, shm_open, mmap, MAP_FAILED
#endif

#include "logger.h" // for log_message, ERROR_MESSAGE
#include "make_stage.h"
#include "xflow_data_types.h" // for DATA_TYPE_DOUBLE

// expand definitions once, using both the decl‐list and the call‐list
MAKE_STAGE_DEFINE(flow_gen, void, (FLOW_GEN_PARAM_LIST), (FLOW_GEN_CALL_ARGS))

// Register the flow_gen implementation
__attribute__((constructor)) static void init_flow_gen_hook(void)
{
	register_flow_gen(csv_fixed_interp_flow_gen);
}

/**
 * @brief Computes and provides a time‐series of flow speed for the aero model using BTS data.
 *
 * On the first invocation, this function binds all necessary parameters from the dynamic
 * and fixed arrays (`flow_speed`, `time_sec`, `dt_sec`, `dur_sec`, `flow_time_step_dt`,
 * `flow_total_time`, `data_processing_first_run`, `data_processing_single_run_only`).
 * If data processing is enabled for the first run or single‐run only:
 *   1. Reads the full BTS file (`readfile_bts`) into a `bts_data_t` structure.
 *   2. Allocates a temporary array `vel_Data` and extracts the hub‐height flow speed time series
 *      via `u_mag_velocity_for_y_z_position`.
 *   3. Computes `total_Time` and stores it back via `update_csv_value()`.
 *   4. Precomputes `num_Sim_Steps = total_Time/dt_sec + 1` samples of interpolated flow speed
 *      at simulation intervals (`dt_sec`) using `interpolate_umag`, storing them in
 *      `precomputed_Flow_Interp`.
 *   5. Creates a shared‐memory segment containing `precomputed_Flow_Interp` for other processes.
 * Otherwise, it attaches to the existing shared‐memory interpolation array.
 *
 * On every call, given the current simulation time `time_sec`, it:
 *   - Computes the exact index `t_sim/dt_sec`, clamping within bounds.
 *   - Uses the precomputed value if the time falls exactly on a grid point, or
 *     on‐the‐fly interpolation if not.
 *   - Assigns the resulting flow speed into `*flow_Speed`.
 *   - If the simulation time exceeds `total_Time`, either holds the last value or
 *     sets `shutdownFlag`, depending on `FLOW_RUN_AFTER_END`.
 *   - On shutdown, cleans up the shared memory and local buffers if this was the first run.
 *
 * @param dynamic_data  Pointer to the parameter array holding dynamic (state) variables.
 * @param fixed_data    Pointer to the parameter array holding fixed configuration variables.
 */
void bts_fixed_interp_flow_gen(FLOW_GEN_PARAM_LIST)
{
	static double *flow_Speed = NULL;
	static double *time_Sec = NULL;          // sim current time, updated outside this funciton.
	static double *flow_Time_Step_Dt = NULL; // flow series time step
	static double *dt_Sec = NULL;            // simulation time step
	static double *dur_Sec = NULL;

	static double *vel_Data = NULL;
	static int vel_Data_Count = 0;

	static bts_data_t bts_Data;

	static double total_Time = 0;
	static double *flow_Total_Time = NULL;

	// New: precomputed array for flow interpolation values at simulation time steps.
	static double *precomputed_Flow_Interp = NULL;
	static int num_Sim_Steps = 0;

	static int *data_Processing_First_Run = NULL;
	static int *data_Processing_Single_Run_Only = NULL;

	static const char *flow_Gen_File_Location_And_Or_Name = NULL;

	static bool first_Run = false;
	if (!first_Run)
	{
		// initialize variables since this is the first time the function is running.

		get_param(dynamic_data, "flow_speed", &flow_Speed);
		get_param(dynamic_data, "time_sec", &time_Sec);
		get_param(fixed_data, "dt_sec", &dt_Sec);
		get_param(fixed_data, "dur_sec", &dur_Sec);
		get_param(fixed_data, "flow_time_step_dt", &flow_Time_Step_Dt);
		get_param(dynamic_data, "flow_total_time", &flow_Total_Time);

		get_param(fixed_data, "data_processing_first_run", &data_Processing_First_Run);
		get_param(fixed_data, "data_processing_single_run_only", &data_Processing_Single_Run_Only);

		get_param(fixed_data, "flow_gen_file_location_and_or_name", &flow_Gen_File_Location_And_Or_Name);

		if (*data_Processing_First_Run || *data_Processing_Single_Run_Only)
		{

#ifndef FLOW_GEN_FILE_DIR
			ERROR_MESSAGE("FLOW_GEN_FILE_DIR needs to be defined through cmake, exiting...\n");
			shutdownFlag = 1;
			return;
#endif
			char flow_filename[PATH_MAX];
			create_dynamic_file_path(flow_filename, PATH_MAX, "%s/%s", FLOW_GEN_FILE_DIR, flow_Gen_File_Location_And_Or_Name);
			if (strlen(flow_filename) < 4 || strcmp(flow_filename + strlen(flow_filename) - 4, ".bts") != 0)
			{
				ERROR_MESSAGE("Log file '%s' must end in .bts\n", flow_filename);
				shutdownFlag = 1;
				return;
			}
			readfile_bts(flow_filename, "int16_t", &bts_Data);
			// save_velocity_to_csv(&bts_Data, 0, -1, OUTPUT_LOG_FILE_PATH, "bts");
			// pass negative 1 for hub height
			// print_velocity_for_y_z_position(&bts_Data, 0, -1);
			// print_velocity_for_yz(&bts_Data, 12, 12);
			vel_Data = (double *)malloc(bts_Data.nt * sizeof(double));

			if (vel_Data == NULL)
			{
				ERROR_MESSAGE("Error: Could not allocate vel_Data.\n");
				shutdownFlag = 1;
				return;
			}

			vel_Data_Count = bts_Data.nt;

			// Call the function
			u_mag_velocity_for_y_z_position(&bts_Data, 0, -1, vel_Data);

			// save_umag_velocity_data_to_csv(vel_Data, bts_Data.nt, OUTPUT_LOG_FILE_PATH, "csv", bts_Data.dt);

			// Calculate the total available time
			total_Time = bts_Data.nt * bts_Data.dt;
			*flow_Total_Time = total_Time;

			// Precompute flow interpolation values for each simulation time step.
			// Number of simulation steps is based on total_Time and dt_sec.
			num_Sim_Steps = (int)(total_Time / (*dt_Sec)) + 1;
			precomputed_Flow_Interp = (double *)malloc(num_Sim_Steps * sizeof(double));
			if (precomputed_Flow_Interp == NULL)
			{
				ERROR_MESSAGE("Error: Could not allocate precomputed_Flow_Interp.\n");
				shutdownFlag = 1;
				return;
			}
			for (int i = 0; i < num_Sim_Steps; i++)
			{
				double sim_time = i * (*dt_Sec);
				// Use your existing interpolation function to compute the flow speed at sim_time.
				precomputed_Flow_Interp[i] = interpolate_umag(vel_Data, bts_Data.nt, sim_time, bts_Data.dt);
			}

			update_csv_value(SYSTEM_CONFIG_FULL_PATH, "flow_total_time", INPUT_PARAM_DOUBLE, &total_Time);

			create_shared_interp(precomputed_Flow_Interp, num_Sim_Steps);
		}
		else
		{
			total_Time = *flow_Total_Time;
			num_Sim_Steps = (int)(total_Time / (*dt_Sec)) + 1;

			// Access the precomputed interpolation data.
			precomputed_Flow_Interp = get_shared_interp(shmemName, num_Sim_Steps);

			if (precomputed_Flow_Interp == NULL)
			{
				ERROR_MESSAGE("Error: Could not allocate precomputed_Flow_Interp.\n");
				shutdownFlag = 1;
				return;
			}

			// Use precomputed_Flow_Interp as needed in your simulation.
			// For example, printing the first value:
			// log_message("First interpolation value: %f\n", precomputed_Flow_Interp[0]);
		}
		first_Run = true;
	}

	// current simulation time in seconds
	double t_sim = *time_Sec;
	double idx_fp = t_sim / (*dt_Sec);
	double idx_round = round(idx_fp);

	// clamp to bounds
	if (idx_round < 0)
	{
		idx_round = 0;
	}
	else if (idx_round > num_Sim_Steps - 1)
	{
		idx_round = num_Sim_Steps - 1;
	}

	if (fabs(idx_fp - idx_round) < 1e-9)
	{
		// “exact” multiple of dt: use precomputed
		int idx = (int)idx_round;
		*flow_Speed = precomputed_Flow_Interp[idx];
		// log_message("no interp (snapped), idx_fp≈%f, idx=%d\n", idx_fp, idx);
	}
	else
	{
		// fractional: interpolate on the fly
		*flow_Speed = interpolate_umag(vel_Data, vel_Data_Count, t_sim, *flow_Time_Step_Dt);
		// log_message("interp needed, idx_fp=%f, idx_floor=%d, frac=%f\n", idx_fp, (int)floor(idx_fp), idx_fp - floor(idx_fp));
	}

	// Check if the simulation time exceeds the available flow data time.
	if (*time_Sec > total_Time)
	{
#ifdef FLOW_RUN_AFTER_END
		// after end-of-data: hold last value steady
		int last_idx = num_Sim_Steps - 1;
		*flow_Speed = precomputed_Flow_Interp[last_idx];
#else
		// log_message("Error: Requested time %f exceeds available time %f in flow. Exiting program.\n", *time_Sec, total_Time);
		shutdownFlag = 1;
#endif
	}

	if (shutdownFlag)
	{
		if (*data_Processing_First_Run || *data_Processing_Single_Run_Only)
		{
			free(precomputed_Flow_Interp);
			destroy_shared_interp();
		}
		else
		{
#ifdef _WIN32
			if (!UnmapViewOfFile(precomputed_Flow_Interp))
			{
				ERROR_MESSAGE("UnmapViewOfFile failed (child)");
				exit(EXIT_FAILURE);
			}
#else
			if (munmap(precomputed_Flow_Interp, num_Sim_Steps * sizeof(double)) == -1)
			{
				ERROR_MESSAGE("munmap (child)");
				exit(EXIT_FAILURE);
			}
#endif
			destroy_shared_interp();
		}
	}
}

/**
 * @brief Generates and provides time-series flow data from a CSV file for the aero model.
 *
 * On first invocation, this function:
 * 1. Binds required parameters (`flow_speed`, `time_sec`, `dt_sec`, `dur_sec`,
 *    `flow_time_step_dt`, `flow_total_time`, `data_processing_first_run`,
 *    `data_processing_single_run_only`) via `get_param()`.
 * 2. Validates that `FLOW_CSV_FULL_PATH` is defined; errors out otherwise.
 * 3. If data processing is enabled for the first run or single run only:
 *    - Calls `read_csv_generic()` to read a single-column CSV (flow speed) into a
 *      temporary `double**` (`vel_Data_Temp`) of `num_rows`.
 *    - Flattens `vel_Data_Temp` into a contiguous `vel_Data[]` array (length `num_rows`)
 *      and frees the temporary buffers.
 *    - Sets `vel_Data_Count = num_rows` and computes `total_Time = vel_Data_Count * flow_time_step_dt`.
 *    - Stores `total_Time` back into the parameters via `update_csv_value()`.
 *    - Precomputes `num_Sim_Steps = total_Time/dt_sec + 1` samples by interpolating
 *      `vel_Data` at each simulation time step (`dt_sec`) using `interpolate_umag()`,
 *      storing results in `precomputed_Flow_Interp`.
 *    - Creates a shared-memory segment containing `precomputed_Flow_Interp` for use by
 *      other processes via `create_shared_interp()`.
 * 4. Otherwise (not first run), attaches to existing shared memory via
 *    `get_shared_interp()` to retrieve `precomputed_Flow_Interp`.
 *
 * On every call:
 * - Computes the floating-point index `idx_fp = time_sec/dt_sec`, clamps to [0, num_Sim_Steps-1].
 * - If `idx_fp` is within 1e-9 of an integer, uses the corresponding precomputed value;
 *   otherwise performs on-the-fly interpolation via `interpolate_umag()`.
 * - Assigns the resulting flow speed into `*flow_Speed`.
 * - If `time_sec > total_Time`, either holds the last value (`FLOW_RUN_AFTER_END`) or
 *   sets `shutdownFlag` to terminate.
 * - On shutdown, if this was the initial data-processing run, frees the shared memory
 *   (`destroy_shared_interp()`).
 *
 * @param dynamic_data  Pointer to the `param_array_t` containing dynamic parameters.
 * @param fixed_data    Pointer to the `param_array_t` containing fixed parameters.
 */
void csv_fixed_interp_flow_gen(FLOW_GEN_PARAM_LIST)
{
	static double *flow_Speed = NULL;
	static double *time_Sec = NULL;          // sim current time, updated outside this funciton.
	static double *flow_Time_Step_Dt = NULL; // flow series time step
	static double *dt_Sec = NULL;            // simulation time step
	static double *dur_Sec = NULL;

	static double *vel_Data = NULL;
	static int vel_Data_Count = 0;

	static double **vel_Data_Temp = NULL;
	int num_rows = 0;

	static double total_Time = 0;
	static double *flow_Total_Time = NULL;

	// New: precomputed array for flow interpolation values at simulation time steps.
	static double *precomputed_Flow_Interp = NULL;
	static int num_Sim_Steps = 0;

	static int *data_Processing_First_Run = NULL;
	static int *data_Processing_Single_Run_Only = NULL;

	static const char *flow_Gen_File_Location_And_Or_Name = NULL;

	static bool first_Run = false;
	if (!first_Run)
	{
		// initialize variables since this is the first time the function is running.

		get_param(dynamic_data, "flow_speed", &flow_Speed);
		get_param(dynamic_data, "time_sec", &time_Sec);
		get_param(fixed_data, "dt_sec", &dt_Sec);
		get_param(fixed_data, "dur_sec", &dur_Sec);
		get_param(fixed_data, "flow_time_step_dt", &flow_Time_Step_Dt);
		get_param(dynamic_data, "flow_total_time", &flow_Total_Time);

		get_param(fixed_data, "data_processing_first_run", &data_Processing_First_Run);
		get_param(fixed_data, "data_processing_single_run_only", &data_Processing_Single_Run_Only);

		get_param(fixed_data, "flow_gen_file_location_and_or_name", &flow_Gen_File_Location_And_Or_Name);

#ifndef FLOW_GEN_FILE_DIR
		ERROR_MESSAGE("FLOW_GEN_FILE_DIR needs to be defined through cmake, exiting...\n");
		shutdownFlag = 1;
		return;
#endif
		char flow_filename[PATH_MAX];
		create_dynamic_file_path(flow_filename, PATH_MAX, "%s/%s", FLOW_GEN_FILE_DIR, flow_Gen_File_Location_And_Or_Name);
		if (strlen(flow_filename) < 4 || strcmp(flow_filename + strlen(flow_filename) - 4, ".csv") != 0)
		{
			ERROR_MESSAGE("Log file '%s' must end in .csv\n", flow_filename);
			shutdownFlag = 1;
			return;
		}

		if (*data_Processing_First_Run || *data_Processing_Single_Run_Only)
		{
			// Example: call read_csv_generic to retrieve a single-column CSV as double**
			vel_Data_Temp = (double **)read_csv_generic(flow_filename, &num_rows, 1, DATA_TYPE_DOUBLE);
			if (vel_Data_Temp == NULL)
			{
				ERROR_MESSAGE("Error: vel_Data_Temp is NULL, could not read CSV.\n");
				shutdownFlag = 1;
				return;
			}

			// Allocate a single contiguous array to hold all rows in a single column
			vel_Data = (double *)malloc(num_rows * sizeof(double));
			if (vel_Data == NULL)
			{
				ERROR_MESSAGE("Error: Could not allocate vel_Data.\n");
				// Clean up vel_Data_Temp before exiting
				for (int i = 0; i < num_rows; i++)
				{
					free(vel_Data_Temp[i]);
				}
				free((void *)vel_Data_Temp);
				vel_Data_Temp = NULL;

				shutdownFlag = 1;
				return;
			}

			vel_Data_Count = num_rows;

			// Flatten the 2D data (num_rows x 1) into the 1D array vel_Data
			for (int i = 0; i < num_rows; i++)
			{
				vel_Data[i] = vel_Data_Temp[i][0];
			}

			// We can now free vel_Data_Temp since vel_Data is holding the actual numbers
			for (int i = 0; i < num_rows; i++)
			{
				free(vel_Data_Temp[i]); // free each row
			}
			free((void *)vel_Data_Temp); // free the array of pointers
			vel_Data_Temp = NULL;

			// save_umag_velocity_data_to_csv(vel_Data, num_rows, OUTPUT_LOG_FILE_PATH, "csv", *flow_Time_Step_Dt);

			// Calculate the total available time
			total_Time = vel_Data_Count * *flow_Time_Step_Dt;
			*flow_Total_Time = total_Time;

			// Precompute flow interpolation values for each simulation time step.
			// Number of simulation steps is based on total_Time and dt_sec.
			num_Sim_Steps = (int)(total_Time / (*dt_Sec)) + 1;
			precomputed_Flow_Interp = (double *)malloc(num_Sim_Steps * sizeof(double));
			if (precomputed_Flow_Interp == NULL)
			{
				ERROR_MESSAGE("Error: Could not allocate precomputed_Flow_Interp.\n");
				shutdownFlag = 1;
				return;
			}
			for (int i = 0; i < num_Sim_Steps; i++)
			{
				double sim_time = i * (*dt_Sec);
				// Use your existing interpolation function to compute the flow speed at sim_time.
				precomputed_Flow_Interp[i] = interpolate_umag(vel_Data, vel_Data_Count, sim_time, *flow_Time_Step_Dt);
			}

			update_csv_value(SYSTEM_CONFIG_FULL_PATH, "flow_total_time", INPUT_PARAM_DOUBLE, &total_Time);

			create_shared_interp(precomputed_Flow_Interp, num_Sim_Steps);
		}
		else
		{
			total_Time = *flow_Total_Time;
			num_Sim_Steps = (int)(total_Time / (*dt_Sec)) + 1;

			// Access the precomputed interpolation data.
			precomputed_Flow_Interp = get_shared_interp(shmemName, num_Sim_Steps);

			if (precomputed_Flow_Interp == NULL)
			{
				ERROR_MESSAGE("Error: Could not allocate precomputed_Flow_Interp.\n");
				shutdownFlag = 1;
				return;
			}

			// Use precomputed_Flow_Interp as needed in your simulation.
			// For example, printing the first value:
			// log_message("First interpolation value: %f\n", precomputed_Flow_Interp[0]);
		}

		first_Run = true;
	}

	// current simulation time in seconds
	double t_sim = *time_Sec;
	double idx_fp = t_sim / (*dt_Sec);
	double idx_round = round(idx_fp);

	// clamp to bounds
	if (idx_round < 0)
	{
		idx_round = 0;
	}
	else if (idx_round > num_Sim_Steps - 1)
	{
		idx_round = num_Sim_Steps - 1;
	}

	if (fabs(idx_fp - idx_round) < 1e-9)
	{
		// “exact” multiple of dt: use precomputed
		int idx = (int)idx_round;
		*flow_Speed = precomputed_Flow_Interp[idx];
		// log_message("no interp (snapped), idx_fp≈%f, idx=%d\n", idx_fp, idx);
	}
	else
	{
		// fractional: interpolate on the fly
		*flow_Speed = interpolate_umag(vel_Data, vel_Data_Count, t_sim, *flow_Time_Step_Dt);
		// log_message("interp needed, idx_fp=%f, idx_floor=%d, frac=%f\n", idx_fp, (int)floor(idx_fp), idx_fp - floor(idx_fp));
	}

	// if (*flow_Speed > 6.0)
	// {
	// 	*flow_Speed = 6.0;
	// }

	// *flow_Speed = *flow_Speed / 2.5;

	// Check if the simulation time exceeds the available flow data time.
	if (*time_Sec > total_Time)
	{
#ifdef FLOW_RUN_AFTER_END
		// after end-of-data: hold last value steady
		int last_idx = num_Sim_Steps - 1;
		*flow_Speed = precomputed_Flow_Interp[last_idx];
#else
		// log_message("Error: Requested time %f exceeds available time %f in flow. Exiting program.\n", *time_Sec, total_Time);
		shutdownFlag = 1;
#endif
	}

	if (shutdownFlag)
	{
		if (*data_Processing_First_Run || *data_Processing_Single_Run_Only)
		{
			free(precomputed_Flow_Interp);
			destroy_shared_interp();
		}
		else
		{
#ifdef _WIN32
			if (!UnmapViewOfFile(precomputed_Flow_Interp))
			{
				ERROR_MESSAGE("UnmapViewOfFile failed (child)\n");
				exit(EXIT_FAILURE);
			}
#else
			if (munmap(precomputed_Flow_Interp, num_Sim_Steps * sizeof(double)) == -1)
			{
				ERROR_MESSAGE("munmap (child)\n");
				exit(EXIT_FAILURE);
			}
#endif
			destroy_shared_interp();
		}
	}
}
