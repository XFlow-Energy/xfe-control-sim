/**
 * @file    xfe_control_sim_common.c
 * @author  XFlow Energy
 * @date    2025
 * @brief   Common functions used for the simulation software specifically
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
/**
 * @file xfe_control_sim_common.c
 * @copyright 2024 XFlow Energy Company
 *
 * @brief APIs commonly used by all application modules.
 */

// NOLINTBEGIN(llvm-include-order)
#include "xflow_file_socket.h"
#include "logger.h"       // for safe_fprintf, log_message, safe_snprintf
#include "maybe_unused.h" // for MAYBE_UNUSED
#include "xfe_control_sim_common.h"
#include "xflow_aero_sim.h"  // for param_array_t, (anonymous struct)::(an...
#include "xflow_core.h"      // for get_monotonic_timestamp, shutdownFlag
#include "xflow_shmem_sem.h" // for shmem_post_check, shmem_wait_check
#include <limits.h>          // for PATH_MAX
#include <math.h>            // for round, sqrt
#include <stdbool.h>         // IWYU pragma: keep
#include <stdio.h>           // for fclose, NULL, fopen, FILE, fwrite, size_t
#include <stdlib.h>          // for exit, EXIT_FAILURE
#include <string.h>          // for strerror, strcmp
#include <time.h>            // for timespec

#ifdef _WIN32
#include <inttypes.h>
#include <windows.h> // For MAX_PATH, CreateFileMapping, MapViewOfFile, etc.
// NOLINTEND(llvm-include-order)
#else
// POSIX-specific headers for shared memory operations
#include <errno.h>    // for errno
#include <fcntl.h>    // for O_RDWR, O_CREAT, O_TRUNC, etc.
#include <sys/mman.h> // for mmap, munmap, shm_open, shm_unlink
#include <sys/wait.h> // For waitpid, WIFEXITED, WEXITSTATUS, etc.
#include <unistd.h>   // for ftruncate, close
#endif
#ifdef __APPLE__
#include <mach/host_info.h>   // for HOST_CPU_LOAD_INFO, HOST_CPU_LOAD_INFO...
#include <mach/kern_return.h> // for KERN_SUCCESS
#include <mach/mach_host.h>   // for host_statistics
#include <mach/mach_init.h>   // for mach_host_self
#include <mach/machine.h>     // for CPU_STATE_IDLE, CPU_STATE_NICE, CPU_ST...
#include <mach/message.h>     // for mach_msg_type_number_t
#include <stdint.h>           // for uint64_t
#include <sys/types.h>        // for pid_t
#endif

// IWYU pragma: no_include <__stdarg_va_arg.h>

#ifdef _WIN32
static HANDLE gHMapFile = NULL; // Global handle to keep mapping alive
#endif

#ifndef DELETE_LOG_FILE_NEW_RUN
#define DELETE_LOG_FILE_NEW_RUN 0
#endif

/**
 * @brief Exports time series of velocity components and magnitude at a given grid point to CSV files.
 *
 * Finds the nearest grid indices for the specified horizontal (y) and vertical (z) positions,
 * then opens four CSV files in the directory `file_path` with names based on `base_filename`:
 *   - `<base_filename>_velocity_abs.csv`  (|V| = √(U²+V²+W²))
 *   - `<base_filename>_velocity_u.csv`    (U component)
 *   - `<base_filename>_velocity_v.csv`    (V component)
 *   - `<base_filename>_velocity_w.csv`    (W component)
 * Each file receives a header line (`Time,Value`) followed by one line per time step:
 * the elapsed time (it × dt) and the corresponding velocity value.
 *
 * If any file cannot be opened, logs an error and returns early, closing any previously opened files.
 *
 * @param data                     Pointer to a populated `bts_data_t` structure.
 * @param horizontal_y_position    Desired horizontal y-coordinate (m) in the grid.
 * @param vertical_z_position      Desired vertical z-coordinate (m) in the grid; use –1 to select hub height.
 * @param file_path                Directory path in which to create the CSV files.
 * @param base_filename            Base filename (without extension) used to construct the CSV filenames.
 *
 * @note
 * - Relies on `find_bts_y_z_position()` to determine grid indices.
 * - Uses `data->velocity` (size nt×3×ny×nz) and grid metadata (`ny`, `nz`, `dt`, `nt`).
 * - Requires `<stdio.h>` for file I/O and `<math.h>` for `sqrt()`.
 */
void save_velocity_to_csv(bts_data_t *data, const double horizontal_y_position, const double vertical_z_position, const char *file_path, const char *base_filename)
{
	char abs_file_filename[256];
	char x_file_filename[256];
	char y_file_filename[256];
	char z_file_filename[256];
	FILE *abs_file = NULL;
	FILE *x_file = NULL;
	FILE *y_file = NULL;
	FILE *z_file = NULL;

	int iy = -1;
	int iz = -1;
	find_bts_y_z_position(data, horizontal_y_position, vertical_z_position, &iy, &iz);

	// Open CSV files for writing
	safe_snprintf(abs_file_filename, sizeof(abs_file_filename), "%s/%s_velocity_abs.csv", file_path, base_filename);
	abs_file = xflow_fopen_safe(abs_file_filename, XFLOW_FILE_WRITE_ONLY);
	if (abs_file == NULL)
	{
		log_message("Error opening file for absolute velocity.\n");
		goto cleanup;
	}

	safe_snprintf(x_file_filename, sizeof(x_file_filename), "%s/%s_velocity_u.csv", file_path, base_filename);
	x_file = xflow_fopen_safe(x_file_filename, XFLOW_FILE_WRITE_ONLY);
	if (x_file == NULL)
	{
		log_message("Error opening file for velocity u.\n");
		goto cleanup;
	}

	safe_snprintf(y_file_filename, sizeof(y_file_filename), "%s/%s_velocity_v.csv", file_path, base_filename);
	y_file = xflow_fopen_safe(y_file_filename, XFLOW_FILE_WRITE_ONLY);
	if (y_file == NULL)
	{
		log_message("Error opening file for velocity v.\n");
		goto cleanup;
	}

	safe_snprintf(z_file_filename, sizeof(z_file_filename), "%s/%s_velocity_w.csv", file_path, base_filename);
	z_file = xflow_fopen_safe(z_file_filename, XFLOW_FILE_WRITE_ONLY);
	if (z_file == NULL)
	{
		log_message("Error opening file for velocity w.\n");
		goto cleanup;
	}

	// Write headers for each file
	safe_fprintf(abs_file, "Time,Velocity_Abs\n");
	safe_fprintf(x_file, "Time,Velocity_U\n");
	safe_fprintf(y_file, "Time,Velocity_V\n");
	safe_fprintf(z_file, "Time,Velocity_W\n");

	// Log header
	log_message("Saving velocity data to csv for index:: iz: %d, iy: %d\n", iz, iy);

	// Loop through all time steps
	int total_grid_points = data->ny * data->nz; // Total grid points in y-z space
	for (int it = 0; it < data->nt; it++)
	{
		double time = it * data->dt; // Time for this step

		// Calculate indices for U, V, and W components at the given (iy, iz) for this time step
		int idx_u = (it * 3 * total_grid_points) + (0 * total_grid_points) + (iy * data->nz) + iz;
		int idx_v = (it * 3 * total_grid_points) + (1 * total_grid_points) + (iy * data->nz) + iz;
		int idx_w = (it * 3 * total_grid_points) + (2 * total_grid_points) + (iy * data->nz) + iz;

		// Get U, V, W components
		double vx = data->velocity[idx_u]; // U-component (X)
		double vy = data->velocity[idx_v]; // V-component (Y)
		double vz = data->velocity[idx_w]; // W-component (Z)
		// Compute absolute velocity
		double v_abs = sqrt((vx * vx) + (vy * vy) + (vz * vz));
		safe_fprintf(abs_file, "%f,%f\n", time, v_abs);
		safe_fprintf(x_file, "%f,%f\n", time, vx);
		safe_fprintf(y_file, "%f,%f\n", time, vy);
		safe_fprintf(z_file, "%f,%f\n", time, vz);
	}

	log_message("CSV files saved successfully.\n");

cleanup:
	// This single cleanup block is always reached, ensuring all opened files are closed.
	if (abs_file)
	{
		if (fclose(abs_file) == EOF)
		{
			ERROR_MESSAGE("Error closing %s: %s\n", abs_file_filename, safe_strerror(errno));
		}
	}
	if (x_file)
	{
		if (fclose(x_file) == EOF)
		{
			ERROR_MESSAGE("Error closing %s: %s\n", x_file_filename, safe_strerror(errno));
		}
	}
	if (y_file)
	{
		if (fclose(y_file) == EOF)
		{
			ERROR_MESSAGE("Error closing %s: %s\n", y_file_filename, safe_strerror(errno));
		}
	}
	if (z_file)
	{
		if (fclose(z_file) == EOF)
		{
			ERROR_MESSAGE("Error closing %s: %s\n", z_file_filename, safe_strerror(errno));
		}
	}
}

/**
 * @brief Logs the time series of velocity components at a specified grid point.
 *
 * Validates the provided horizontal (`iy`) and vertical (`iz`) indices against
 * the BTS grid dimensions. If valid, prints a header and then, for each time step,
 * logs the elapsed time and the U (X), V (Y), and W (Z) velocity components at
 * that grid location. The array indices used for each component and time step
 * are also included in the log for debugging.
 *
 * @param data  Pointer to a populated `bts_data_t` structure containing:
 *              - `velocity` array of size `nt × 3 × ny × nz`.
 *              - Grid dimensions `ny`, `nz`, time step count `nt`, and interval `dt`.
 * @param iy    Horizontal grid index (0 ≤ iy < data->ny).
 * @param iz    Vertical grid index (0 ≤ iz < data->nz).
 *
 * @note
 * - If `iy` or `iz` is out of range, the function logs an error and returns without output.
 * - Relies on `log_message()` for output formatting.
 */
void print_velocity_for_yz(bts_data_t *data, int iy, int iz)
{
	if (iy >= data->ny || iz >= data->nz || iy < 0 || iz < 0)
	{
		log_message("Invalid y or z index\n");
		return;
	}

	log_message("Time, U (Velocity X), V (Velocity Y), W (Velocity Z), iz: %d, iy: %d\n", iz, iy);

	// Loop through all time steps
	int total_grid_points = data->ny * data->nz; // Total grid points in y-z space
	for (int it = 0; it < data->nt; it++)
	{
		double time = it * data->dt; // Time for this step

		// Calculate indices for U, V, and W components at the given (iy, iz) for this time step
		int idx_u = (it * 3 * total_grid_points) + (0 * total_grid_points) + (iy * data->nz) + iz;
		int idx_v = (it * 3 * total_grid_points) + (1 * total_grid_points) + (iy * data->nz) + iz;
		int idx_w = (it * 3 * total_grid_points) + (2 * total_grid_points) + (iy * data->nz) + iz;

		// Get U, V, W components
		double vx = data->velocity[idx_u]; // U-component (X)
		double vy = data->velocity[idx_v]; // V-component (Y)
		double vz = data->velocity[idx_w]; // W-component (Z)
		// Print time and velocity components
		log_message("%f, %f(%d), %f(%d), %f(%d)\n", time, vx, idx_u, vy, idx_v, vz, idx_w);
	}
}

/**
 * @brief Logs the time series of velocity components at the nearest grid point for given coordinates.
 *
 * Determines the grid indices corresponding to the specified horizontal (y) and vertical (z)
 * positions using `find_bts_y_z_position()`, then iterates over all time steps to log:
 * - Elapsed time (`it * dt`)
 * - U (X), V (Y), and W (Z) velocity components at that location.
 *
 * @param data                    Pointer to the `bts_data_t` structure containing:
 *                                - `velocity` array of size `nt × 3 × ny × nz`
 *                                - Grid dimensions (`ny`, `nz`), time step count (`nt`), and interval (`dt`).
 * @param horizontal_y_position   Desired horizontal y-coordinate (m) in the grid.
 * @param vertical_z_position     Desired vertical z-coordinate (m) in the grid; use –1 to select hub height.
 */
void print_velocity_for_y_z_position(bts_data_t *data, const double horizontal_y_position, const double vertical_z_position)
{
	int iy = -1;
	int iz = -1;
	find_bts_y_z_position(data, horizontal_y_position, vertical_z_position, &iy, &iz);

	// Log header
	log_message("Time, U (Velocity X), V (Velocity Y), W (Velocity Z), iz: %d, iy: %d\n", iz, iy);

	// Total grid points in y-z space
	int total_grid_points = data->ny * data->nz;

	// Loop through all time steps
	for (int it = 0; it < data->nt; it++)
	{
		double time = it * data->dt; // Time for this step

		// Calculate indices for U, V, and W components at the given (iy, iz) for this time step
		int idx_u = (it * 3 * total_grid_points) + (0 * total_grid_points) + (iy * data->nz) + iz;
		int idx_v = (it * 3 * total_grid_points) + (1 * total_grid_points) + (iy * data->nz) + iz;
		int idx_w = (it * 3 * total_grid_points) + (2 * total_grid_points) + (iy * data->nz) + iz;

		// Get U, V, W components
		double vx = data->velocity[idx_u]; // U-component (X)
		double vy = data->velocity[idx_v]; // V-component (Y)
		double vz = data->velocity[idx_w]; // W-component (Z)

		// Print time and velocity components
		log_message("%f, %f, %f, %f\n", time, vx, vy, vz);
	}
}

/**
 * @brief Exports a time series of wind speed magnitudes to a CSV file.
 *
 * Creates a CSV file named `<base_filename>_velocity_umag.csv` in the specified
 * `file_path` directory. Writes a header line (`Time,U_mag`) followed by one
 * line per time step containing the elapsed time (`it * dt`) and the corresponding
 * velocity magnitude from `vel_data`.
 *
 * @param vel_data         Array of length `num_time_steps` containing wind speed magnitudes.
 * @param num_time_steps   Number of time steps (rows) in `vel_data`.
 * @param file_path        Directory path in which to create the CSV file.
 * @param base_filename    Base filename (without extension) used for the output file.
 * @param dt               Time interval between samples, used to compute the Time column.
 *
 * @note
 * - If the file cannot be opened for writing, logs an error via `ERROR_MESSAGE()` and returns without writing.
 * - Requires `<stdio.h>` for file I/O and `log_message()` for logging.
 */
void save_umag_velocity_data_to_csv(const double *vel_data, int num_time_steps, const char *file_path, const char *base_filename, double dt)
{
	char filename[256];
	safe_snprintf(filename, sizeof(filename), "%s/%s_velocity_umag.csv", file_path, base_filename);

	FILE *file = xflow_fopen_safe(filename, XFLOW_FILE_WRITE_ONLY);
	if (file == NULL)
	{
		ERROR_MESSAGE("Error: Could not open file %s for writing\n", filename);
		return;
	}

	// Write CSV header
	safe_fprintf(file, "Time,U_mag\n");

	// Loop through the data and write it to the file
	for (int it = 0; it < num_time_steps; it++)
	{
		double time = it * dt; // Calculate the time based on the index
		safe_fprintf(file, "%f,%f\n", time, vel_data[it]);
	}

	// Close the file
	if (fclose(file) == EOF)
	{
		ERROR_MESSAGE("Error closing %s: %s\n", filename, safe_strerror(errno));
	}

	log_message("Data saved to %s\n", filename);
}

/**
 * @brief Retrieves the nearest wind speed magnitude for a given time by rounding to the closest sample.
 *
 * Computes the index corresponding to `current_time` by rounding `(current_time / dt)` to the nearest integer,
 * clamps the index to the valid range [0, num_time_steps-1], and returns the velocity magnitude at that index.
 *
 * @param vel_data         Array of length `num_time_steps` containing wind speed magnitudes.
 * @param num_time_steps   Total number of time steps in `vel_data` (must be ≥ 1).
 * @param current_time     Time (in same units as `dt`) for which to retrieve the closest magnitude.
 * @param dt               Time interval between successive entries in `vel_data`.
 * @return                 The velocity magnitude from `vel_data` at the time sample closest to `current_time`.
 *
 * @note
 * - If `current_time` is before the first sample (index < 0), returns the first element.
 * - If `current_time` is after the last sample, returns the last element.
 */
double get_closest_umag(const double *vel_data, int num_time_steps, double current_time, double dt)
{
	// Calculate the closest time index
	int closest_index = (int)round(current_time / dt);

	// Ensure the index is within bounds
	if (closest_index < 0)
	{
		closest_index = 0;
	}
	else if (closest_index >= num_time_steps)
	{
		closest_index = num_time_steps - 1;
	}

	// Return the velocity magnitude at the closest time step
	return vel_data[closest_index];
}

/**
 * @brief Appends or writes a parameter array snapshot to a CSV file with timestamp.
 *
 * Opens the specified CSV file for writing (with header) or appending (without header),
 * writes an optional header row of parameter names preceded by "epoch_time", then
 * writes a data row containing the current real-time timestamp (seconds.nanoseconds)
 * and the values of each parameter in the `param_array_t`. Supports integer, double,
 * and string parameter types.
 *
 * @param filename      Path to the CSV file to write or append.
 * @param data          Pointer to the `param_array_t` containing parameters to log.
 * @param write_header  If non-zero, opens the file in write mode and writes the header row;
 *                      otherwise opens in append mode and skips the header.
 *
 * @note
 * - Uses `get_monotonic_timestamp()` to obtain the current time since boot.
 * - Timestamp is formatted using `TIME_FORMAT` for seconds and `.%.5ld` for fractional seconds.
 * - Requires `<stdio.h>` for I/O and `<time.h>` for `struct timespec`.
 */
void save_param_array_data_to_csv(const char *filename, const param_array_t *data, int write_header)
{
	// Determine the correct file mode based on whether a header should be written.
	xflow_file_mode_t mode = write_header ? XFLOW_FILE_WRITE_ONLY : XFLOW_FILE_APPEND;

	FILE *file = xflow_fopen_safe(filename, mode);
	if (!file)
	{
		ERROR_MESSAGE("Failed to open file for writing: %s\n", filename);
		return;
	}

	if (write_header)
	{
		// Write the header row with epoch time as the first column and the parameter names
		safe_fprintf(file, "epoch_time");
		for (int i = 0; i < data->n_param; i++)
		{
			safe_fprintf(file, ",%s", data->params[i].name);
		}
		safe_fprintf(file, "\n");
	}

	// Get the current timestamp
	struct timespec ts = get_monotonic_timestamp();

	char line[4096];
	int len = safe_snprintf(line, sizeof(line), TIME_FORMAT ".%.5ld", ts.tv_sec, ts.tv_nsec);
	for (int i = 0; i < data->n_param; i++)
	{
		input_param_t *param = &data->params[i];
		switch (param->type)
		{
		case INPUT_PARAM_INT:
			len += safe_snprintf(line + len, sizeof(line) - len, ",%d", param->value.i);
			break;
		case INPUT_PARAM_DOUBLE:
			len += safe_snprintf(line + len, sizeof(line) - len, ",%.10f", param->value.d);
			break;
		case INPUT_PARAM_STRING:
			len += safe_snprintf(line + len, sizeof(line) - len, ",%s", param->value.s ? param->value.s : "");
			break;
		default:
			ERROR_MESSAGE("Unknown parameter type for %s\n", param->name);
			break;
		}
	}
	line[len++] = '\n';

	// Write the data line to the file
	size_t written = fwrite(line, 1, len, file);
	if (written < (size_t)len)
	{
		ERROR_MESSAGE("Failed to write full data row to %s\n", filename);
	}

	// Close the file
	if (fclose(file) == EOF)
	{
		ERROR_MESSAGE("Error closing %s: %s\n", filename, safe_strerror(errno));
	}
	// log_message("Successfully saved dynamic data row to %s\n", filename);
}

void dynamic_data_csv_logger(const csv_logger_action_t action, const char *filename, const param_array_t *data)
{
	static FILE *file = NULL;
	static char buf[1U << 22U];
	static char line[4096];
	static struct timespec total_Logger_Time = {0, 0};
	struct timespec ts;
	int len = 0;
	size_t written = 0;
	struct timespec start_ts = get_monotonic_timestamp();
	if (action == CSV_LOGGER_INIT)
	{
		file = xflow_fopen_safe(filename, XFLOW_FILE_WRITE_ONLY);
		if (!file)
		{
			ERROR_MESSAGE("Failed to open file for writing: %s\n", filename);
			return;
		}

		if (setvbuf(file, buf, _IOFBF, sizeof(buf)) != 0)
		{
			ERROR_MESSAGE("Failed to set file buffer\n");
		}
		if (safe_fprintf(file, "epoch_time") < 0)
		{
			ERROR_MESSAGE("Failed to write CSV header\n");
		}
		for (int i = 0; i < data->n_param; i++)
		{
			if (safe_fprintf(file, ",%s", data->params[i].name) < 0)
			{
				ERROR_MESSAGE("Failed to write CSV header name\n");
			}
		}
		if (safe_fprintf(file, "\n") < 0)
		{
			ERROR_MESSAGE("Failed to write CSV header newline\n");
		}
		return;
	}

	if (action == CSV_LOGGER_LOG)
	{
		if (!file)
		{
			ERROR_MESSAGE("CSV logger not initialized\n");
			return;
		}
		ts = get_monotonic_timestamp();
		len = safe_snprintf(line, sizeof(line), TIME_FORMAT ".%.5ld", ts.tv_sec, ts.tv_nsec);
		if (len < 0 || len >= (int)sizeof(line))
		{
			ERROR_MESSAGE("Timestamp formatting overflow\n");
			len = sizeof(line) - 1;
		}
		for (int i = 0; i < data->n_param; i++)
		{
			const input_param_t *param = &data->params[i];
			switch (param->type)
			{
			case INPUT_PARAM_INT:
				len += safe_snprintf(line + len, sizeof(line) - len, ",%d", param->value.i);
				break;
			case INPUT_PARAM_DOUBLE:
				len += safe_snprintf(line + len, sizeof(line) - len, ",%.10f", param->value.d);
				break;
			case INPUT_PARAM_STRING:
				len += safe_snprintf(line + len, sizeof(line) - len, ",%s", param->value.s ? param->value.s : "");
				break;
			default:
				ERROR_MESSAGE("Unknown parameter type for %s\n", param->name);
				break;
			}
			if (len < 0 || len >= (int)sizeof(line))
			{
				ERROR_MESSAGE("CSV line formatting overflow\n");
				break;
			}
		}
		line[len++] = '\n';
		written = fwrite(line, 1, len, file);
		if (written != (size_t)len)
		{
			ERROR_MESSAGE("Write error: %zu of %d\n", written, len);
		}
		struct timespec end_ts = get_monotonic_timestamp();
		struct timespec delta = timespec_diff(start_ts, end_ts);
		total_Logger_Time = timespec_add(total_Logger_Time, delta);
		return;
	}

	if (action == CSV_LOGGER_CLOSE)
	{
		if (file)
		{
			if (fflush(file) != 0)
			{
				ERROR_MESSAGE("Failed to flush file\n");
			}
			if (fclose(file) == EOF)
			{
				ERROR_MESSAGE("Error closing %s: %s\n", filename, safe_strerror(errno));
			}
			file = NULL;
		}
		log_message("write Duration: %ld.%.5ld\n", total_Logger_Time.tv_sec, total_Logger_Time.tv_nsec / 10000);
		return;
	}
}

/**
 * @brief Retrieves a parameter’s type and value by name.
 *
 * Searches the parameter array for an entry whose `name` matches the provided string.
 * On success, stores the parameter’s type in `*type`. If `value` is non-NULL,
 * also writes the parameter’s value into the memory pointed to by `value`:
 * - For `INPUT_PARAM_INT`, writes to an `int*`.
 * - For `INPUT_PARAM_DOUBLE`, writes to a `double*`.
 * - For `INPUT_PARAM_STRING`, writes to a `char**` (pointer to the stored string).
 *
 * @param data   Pointer to the `param_array_t` containing parameters.
 * @param name   Null-terminated name of the parameter to retrieve.
 * @param[out] type   Pointer to receive the parameter’s type.
 * @param[out] value  Pointer to memory to receive the parameter’s value; must be cast
 *                    to the appropriate pointer type matching `*type`, or NULL to skip value retrieval.
 * @return       0 on success; -1 if the parameter name is not found or type is unknown.
 */
int get_param_value(const param_array_t *data, const char *name, input_param_type_t *type, void *value)
{
	for (int i = 0; i < data->n_param; i++)
	{
		if (strcmp(data->params[i].name, name) == 0)
		{
			*type = data->params[i].type;
			if (value != NULL) // Only retrieve the value if value is not NULL
			{
				switch (data->params[i].type)
				{
				case INPUT_PARAM_INT:
					*(int *)value = data->params[i].value.i;
					break;
				case INPUT_PARAM_DOUBLE:
					*(double *)value = data->params[i].value.d;
					break;
				case INPUT_PARAM_STRING:
					*(char **)value = data->params[i].value.s;
					break;
				default:
					return -1; // Unknown parameter type
				}
			}
			return 0; // Success
		}
	}
	return -1; // Parameter not found
}

/**
 * @brief Saves dynamic and fixed parameter data to CSV at shutdown based on logging configuration.
 *
 * When `logging_status` is true, conditionally writes the dynamic and/or fixed parameter arrays
 * to their respective CSV files. Controlled by compile-time macros:
 * - If `LOGGING_DYNAMIC_FIXED_DATA_ONCE` and `DYNAMIC_DATA_FULL_PATH` are defined,
 *   calls `save_param_array_data_to_csv(DYNAMIC_DATA_FULL_PATH, dynamic_data, 0)`.
 * - If `LOGGING_DYNAMIC_FIXED_DATA_ONCE` or `LOGGING_DYNAMIC_DATA_CONTINUOUS` and
 *   `FIXED_DATA_FULL_PATH` are defined, calls `save_param_array_data_to_csv(FIXED_DATA_FULL_PATH, fixed_data, 1)`.
 *
 * @param dynamic_data     Pointer to the dynamic `param_array_t` (may be unused).
 * @param fixed_data       Pointer to the fixed `param_array_t` (may be unused).
 * @param logging_status   If true, perform the CSV saving; otherwise do nothing.
 *
 * @note
 * - Both `dynamic_data` and `fixed_data` are marked `MAYBE_UNUSED` to suppress unused-parameter warnings
 *   when the corresponding logging paths are disabled at compile time.
 */
void save_dynamic_fixed_data_at_shutdown(MAYBE_UNUSED const param_array_t *dynamic_data, MAYBE_UNUSED const param_array_t *fixed_data, const bool logging_status)
{
	static bool first_Run = true;
	static int *dynamic_Val_Logging = NULL;
	if (first_Run)
	{
		get_param(fixed_data, "dynamic_val_logging", &dynamic_Val_Logging);
		first_Run = false;
	}

	if (*dynamic_Val_Logging <= 0)
	{
		return;
	}

	if (logging_status)
	{
#if defined(LOGGING_DYNAMIC_FIXED_DATA_ONCE) && defined(DYNAMIC_DATA_FULL_PATH)
		// save_param_array_data_to_csv(DYNAMIC_DATA_FULL_PATH, dynamic_data, 0);
		dynamic_data_csv_logger(CSV_LOGGER_CLOSE, DYNAMIC_DATA_FULL_PATH, dynamic_data);
#endif

#if defined(LOGGING_DYNAMIC_FIXED_DATA_ONCE) || defined(LOGGING_DYNAMIC_DATA_CONTINUOUS) && defined(FIXED_DATA_FULL_PATH)
		save_param_array_data_to_csv(FIXED_DATA_FULL_PATH, fixed_data, 1);
#endif
	}
}

/**
 * @brief Sets up all control system data structures, including parameter arrays, an optimized history update list, and optional logging.
 *
 * Allocates and initializes the dynamic and fixed parameter arrays, then reads the
 * main configuration CSV file to populate them. After populating the arrays, it
 * inspects the dynamic data to create and return an optimized list of tasks for
 * updating parameter histories during the simulation.
 *
 * If `logging_status` is true, it also optionally configures a timestamped log file
 * and writes the initial headers to the dynamic data CSV file if continuous
 * logging is enabled.
 *
 * @param[out] dynamic_data     Address of a `param_array_t*` that will be updated to point to the new dynamic data array.
 * @param[out] fixed_data       Address of a `param_array_t*` that will be updated to point to the new fixed data array.
 * @param[out] out_task_list    Address of a `history_task_list_t*` that will be updated to point to the new history task list. Will be set to NULL if no history tasks are found.
 * @param[in]  logging_status   If true, enable log file initialization and CSV header writing.
 *
 * @note
 * - Calls `create_input_data(1)` to allocate each parameter array with a placeholder entry.
 * - The history task list is created by calling `create_history_update_list` and must be freed by the caller.
 * - If `LOGGING_DYNAMIC_DATA_CONTINUOUS` is defined, writes CSV headers to `DYNAMIC_DATA_FULL_PATH`.
 * - Relies on compile-time macros: `OUTPUT_LOG_FILE_PATH`, `DYNAMIC_DATA_FULL_PATH`,
 * `FIXING_DATA_FULL_PATH`, and `LOGGING_DYNAMIC_DATA_CONTINUOUS`.
 */
void initialize_control_system(param_array_t **dynamic_data, param_array_t **fixed_data, history_task_list_t **out_task_list, const bool logging_status)
{
	// --- Part 1: Create the main data arrays ---
	int n_params = 1;
	*dynamic_data = create_input_data(n_params); //
	*fixed_data = create_input_data(n_params);   //

	// --- Part 2: Populate the main data arrays ---
	set_int_param(*dynamic_data, 0, "initialize", 1); //
	set_int_param(*fixed_data, 0, "initialize", 1);   //

	read_csv_and_store(SYSTEM_CONFIG_FULL_PATH, *dynamic_data, *fixed_data); //

	// --- Part 3: Create the history task list ---
	// The create function is called, and the result is assigned to the output parameter.
	*out_task_list = create_history_update_list(*dynamic_data, *fixed_data);
	// log_message("after initialize_data\n");

	if (logging_status)
	{
		// If the output log file path exists, then we can save the output to a log file for later review.
#ifdef OUTPUT_LOG_FILE_PATH
		int *check_verbose = NULL;
		get_param(*fixed_data, "verbose", &check_verbose);
		if (*check_verbose <= 0)
		{
			return;
		}

		const char *log_file_location_and_or_name = NULL;
		get_param(*fixed_data, "log_file_location_and_or_name", &log_file_location_and_or_name);

		char output_log_filename_xfe_control_sim[PATH_MAX];

		create_dynamic_file_path(output_log_filename_xfe_control_sim, sizeof(output_log_filename_xfe_control_sim), "%s", log_file_location_and_or_name);
		char logfilename[PATH_MAX];
		log_file_ammend_remove_t log_ammend_delete = DELETE_OLD_LOG_FILE;

#if DELETE_LOG_FILE_NEW_RUN == 1
		log_ammend_delete = DELETE_OLD_LOG_FILE;
#else
		log_ammend_delete = AMMEND_LOG_FILE;
#endif
		initialize_log_file(logfilename, PATH_MAX, OUTPUT_LOG_FILE_PATH, output_log_filename_xfe_control_sim, log_ammend_delete);
#endif
		// log_message("after initialize_log_file\n");

		// Initialize the CSV file to save the dynamic data if desired.
#if (defined(LOGGING_DYNAMIC_FIXED_DATA_ONCE) || defined(LOGGING_DYNAMIC_DATA_CONTINUOUS)) && defined(DYNAMIC_DATA_FULL_PATH)
		if (LOGGING_DYNAMIC_DATA_CONTINUOUS)
		{
			// save_param_array_data_to_csv(DYNAMIC_DATA_FULL_PATH, *dynamic_data, 1); // Dereference when passing
			dynamic_data_csv_logger(CSV_LOGGER_INIT, DYNAMIC_DATA_FULL_PATH, *dynamic_data);
		}
#if defined(LOGGING_DYNAMIC_FIXED_DATA_ONCE) || defined(LOGGING_DYNAMIC_DATA_CONTINUOUS) && defined(FIXED_DATA_FULL_PATH)
		save_param_array_data_to_csv(FIXED_DATA_FULL_PATH, *fixed_data, 1);
#endif
#endif
	}
}

/**
 * @brief Periodically logs dynamic parameters to CSV when continuous logging is enabled.
 *
 * If the compile-time macros `LOGGING_DYNAMIC_DATA_CONTINUOUS` and `DYNAMIC_DATA_FULL_PATH`
 * are defined, and at runtime the `dynamic_val_logging` fixed parameter is > 0 and
 * `LOGGING_DYNAMIC_DATA_CONTINUOUS` evaluates to true, this function appends the
 * current dynamic parameter values to the CSV file specified by
 * `DYNAMIC_DATA_FULL_PATH` without rewriting the header.
 *
 * @param dynamic_data  Pointer to the `param_array_t` containing dynamic parameters to log.
 * @param fixed_data    Pointer to the `param_array_t` containing control flags; must include
 *                      `dynamic_val_logging` to control whether logging occurs.
 *
 * @note
 * - If `dynamic_val_logging` ≤ 0, this function returns immediately.
 * - Uses `save_param_array_data_to_csv()` with `write_header = 0` to append data rows.
 */
void continuous_logging_function(const param_array_t *dynamic_data, const param_array_t *fixed_data)
{
	static bool first_Run = true;
	static int *dynamic_Val_Logging = NULL;
	if (first_Run)
	{
		get_param(fixed_data, "dynamic_val_logging", &dynamic_Val_Logging);
		first_Run = false;
	}

	if (*dynamic_Val_Logging <= 0)
	{
		return;
	}

#if defined(LOGGING_DYNAMIC_DATA_CONTINUOUS) && defined(DYNAMIC_DATA_FULL_PATH)
	if (LOGGING_DYNAMIC_DATA_CONTINUOUS)
	{
		dynamic_data_csv_logger(CSV_LOGGER_LOG, DYNAMIC_DATA_FULL_PATH, dynamic_data);
		// save_param_array_data_to_csv(DYNAMIC_DATA_FULL_PATH, dynamic_data, 0);
	}
#endif
}

/**
 * @brief Retrieves a named double parameter and stores it into a variable.
 *
 * Uses `get_param()` to obtain a pointer to the `double` value associated with
 * `param_name` in the parameter array, then dereferences it into `*param`.
 * Exits the program if the parameter is not found or not of type double.
 *
 * @param data        Pointer to the `param_array_t` containing parameters.
 * @param param_name  Null-terminated name of the double parameter to load.
 * @param[out] param  Pointer to a `double` variable where the retrieved value will be stored.
 */
void load_double_struct_param(const param_array_t *data, const char *param_name, double *param)
{
	double *temp_ptr = NULL;
	get_param(data, param_name, &temp_ptr);
	*param = *temp_ptr;
}

/**
 * @brief Creates or resets a shared memory region and populates it with precomputed interpolation data.
 *
 * Allocates a shared memory segment sized for `num_sim_steps` doubles, maps it into the process’s
 * address space, and copies the array `precomputed_wind_interp` into that region. On Windows, uses
 * `CreateFileMapping()` and `MapViewOfFile()`, retaining the mapping handle in `gHMapFile`. On POSIX,
 * unlinks any existing object, then uses `shm_open()`, `ftruncate()`, and `mmap()` before unmapping.
 * Logs an error and terminates the program on failure.
 *
 * @param precomputed_wind_interp  Pointer to an array of `num_sim_steps` doubles containing
 *                                 the precomputed interpolation values to share.
 * @param num_sim_steps            Number of steps (elements) in the `precomputed_wind_interp` array.
 */
void create_shared_interp(const double *precomputed_wind_interp, int num_sim_steps)
{
	size_t shm_size = num_sim_steps * sizeof(double);

#ifdef _WIN32
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	gHMapFile = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		&sa,
		PAGE_READWRITE,
		0,
		(DWORD)shm_size,
		shmemName
	);
	if (gHMapFile == NULL)
	{
		ERROR_MESSAGE("CreateFileMapping failed: %ld\n", GetLastError());
		exit(EXIT_FAILURE);
	}

	LPVOID p_buf = MapViewOfFile(
		gHMapFile,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		shm_size
	);
	if (p_buf == NULL)
	{
		ERROR_MESSAGE("MapViewOfFile failed: %ld\n", GetLastError());
		CloseHandle(gHMapFile);
		exit(EXIT_FAILURE);
	}

	// Copy the precomputed data into shared memory.
	safe_memcpy(p_buf, shm_size, precomputed_wind_interp, shm_size);
	log_message("Just created %s\n", shmemName);

	// Unmap the view and close the handle.
	// if (!UnmapViewOfFile(p_buf))
	// {
	// 	ERROR_MESSAGE("UnmapViewOfFile failed: %ld\n", GetLastError());
	// 	CloseHandle(gHMapFile);
	// 	exit(EXIT_FAILURE);
	// }
	// CloseHandle(gHMapFile);

#else
	// POSIX: Remove any existing shared memory object.
	shm_unlink(shmemName);

	// Create (or open) a shared memory object.
	int shm_fd = shm_open(shmemName, O_CREAT | O_RDWR, 0666);
	if (shm_fd == -1)
	{
		ERROR_MESSAGE("shm_open failed: %s\n", safe_strerror(errno));
		exit(EXIT_FAILURE);
	}

	// Set the size of the shared memory object.
	if (ftruncate(shm_fd, (off_t)shm_size) == -1)
	{
		ERROR_MESSAGE("ftruncate failed: %s\n", safe_strerror(errno));
		close(shm_fd);
		exit(EXIT_FAILURE);
	}

	// Map the shared memory into the parent's address space.
	double *shm_ptr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (shm_ptr == MAP_FAILED)
	{
		ERROR_MESSAGE("mmap failed: %s\n", safe_strerror(errno));
		close(shm_fd);
		exit(EXIT_FAILURE);
	}

	// Copy the precomputed data into shared memory.
	if (safe_memcpy(shm_ptr, shm_size, precomputed_wind_interp, shm_size) != 0)
	{
		ERROR_MESSAGE("shared memory copy failed");
		exit(EXIT_FAILURE);
	}
	log_message("Just created %s\n", shmemName);

	// Unmap the shared memory since it's populated.
	if (munmap(shm_ptr, shm_size) == -1)
	{
		ERROR_MESSAGE("munmap failed: %s\n", safe_strerror(errno));
		close(shm_fd);
		exit(EXIT_FAILURE);
	}
	close(shm_fd);
#endif
}

/**
 * @brief Destroys the shared interpolation memory segment.
 *
 * On Windows, the shared memory is automatically released when all handles are closed.
 * On POSIX systems, unlinks the shared memory object named `shmemName`, removing it from the system.
 * Logs an error and exits on failure.
 */
void destroy_shared_interp(void)
{
#ifdef _WIN32
	// No explicit destroy/unlink is required on Windows.
	log_message("Destroying shared memory is handled automatically in Windows.\n");
#else
	if (shm_unlink(shmemName) == -1)
	{
		ERROR_MESSAGE("shm_unlink failed: %s\n", safe_strerror(errno));
		exit(EXIT_FAILURE);
	}
#endif
}

/**
 * @brief Opens and maps a shared memory segment containing precomputed interpolation data.
 *
 * On Windows, uses `OpenFileMapping()` and `MapViewOfFile()` to open and map the shared memory
 * object named `name` for read access, then closes the mapping handle.
 * On POSIX systems, calls `shm_open()` with read-only flags and uses `mmap()` to map the object.
 * In both cases, the returned pointer references a memory region of size `num_sim_steps * sizeof(double)`,
 * containing the shared interpolation data.
 *
 * @param name            Null-terminated name of the shared memory object to open.
 * @param num_sim_steps   Number of `double` elements in the shared memory region.
 * @return                Pointer to the mapped `double` array on success;
 *                        the process must not unmap or unlink it here.
 *
 * @note
 * - On Windows, the returned pointer remains valid until the view is unmapped (caller’s responsibility).
 * - On POSIX, the caller should call `munmap()` when finished.
 * - On any failure, logs an error via `ERROR_MESSAGE()` and calls `exit(EXIT_FAILURE)`.
 */
double *get_shared_interp(const char *name, int num_sim_steps)
{
	size_t shm_size = num_sim_steps * sizeof(double);
#ifdef _WIN32
	HANDLE h_map_file = OpenFileMapping(
		FILE_MAP_READ,
		FALSE,
		name
	);
	if (h_map_file == NULL)
	{
		ERROR_MESSAGE("OpenFileMapping failed: %ld\n", GetLastError());
		exit(EXIT_FAILURE);
	}

	LPVOID p_buf = MapViewOfFile(
		h_map_file,
		FILE_MAP_READ,
		0,
		0,
		shm_size
	);
	if (p_buf == NULL)
	{
		ERROR_MESSAGE("MapViewOfFile failed: %ld\n", GetLastError());
		CloseHandle(h_map_file);
		exit(EXIT_FAILURE);
	}
	CloseHandle(h_map_file);
	return (double *)p_buf;
#else
	int shm_fd = shm_open(name, O_RDONLY, 0666);
	if (shm_fd == -1)
	{
		ERROR_MESSAGE("shm_open (child) failed: %s\n", safe_strerror(errno));
		exit(EXIT_FAILURE);
	}

	double *interp_data = mmap(NULL, shm_size, PROT_READ, MAP_SHARED, shm_fd, 0);
	if (interp_data == MAP_FAILED)
	{
		ERROR_MESSAGE("mmap (child) failed: %s\n", safe_strerror(errno));
		close(shm_fd);
		exit(EXIT_FAILURE);
	}
	close(shm_fd);
	return interp_data;
#endif
}

/**
 * @brief Stores a value into a flattened 2D array at a specified row and column.
 *
 * Calculates the linear index in a 1D array representing a matrix with
 * `final_dp_index` columns and `sim_points_count + 1` rows, then writes
 * the value pointed to by `value_ptr` into that position.
 *
 * @param array               Pointer to the 1D array storing 2D data.
 * @param sim_points_count    Number of simulation points (rows).
 * @param index               Column index within the row (0-based).
 * @param final_dp_index      Total number of columns per row.
 * @param value_ptr           Pointer to the double value to insert.
 *
 * @note
 * - The array must have at least `(sim_points_count + 1) * final_dp_index` elements.
 * - The value is written at `array[sim_points_count * final_dp_index + index]`.
 */
void add_data_to_array(double *array, const long sim_points_count, const int index, const int final_dp_index, const double *value_ptr)
{
	// Each row has (final_dp_index - 1) columns.
	// We dereference value_ptr to obtain the value to store.
	long final_index = (sim_points_count * final_dp_index) + index;
	array[final_index] = *value_ptr;
}

/**
 * @brief Returns the number of CPU cores available to the process.
 *
 * On Windows (`_WIN32`), retrieves system information via `GetSystemInfo()` and
 * returns `dwNumberOfProcessors`. On POSIX systems, uses `sysconf(_SC_NPROCESSORS_ONLN)`
 * to determine the number of online processors.
 *
 * @return Number of logical CPU cores available, or a negative value on error.
 *
 * @note
 * - Requires `<windows.h>` for Windows and `<unistd.h>` for POSIX.
 * - On failure, POSIX `sysconf()` returns -1; Windows `GetSystemInfo()` always succeeds.
 */
int get_num_cores(void)
{
#ifdef _WIN32
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return (int)sysinfo.dwNumberOfProcessors;
#else
	return (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

/**
 * @brief Comparator function for sorting an array of doubles with `qsort()`.
 *
 * Interprets the pointers `a` and `b` as pointers to `double` values,
 * compares them, and returns:
 * - Negative value if *a < *b
 * - Positive value if *a > *b
 * - Zero if *a == *b
 *
 * @param a  Pointer to the first element to compare (must point to a `double`).
 * @param b  Pointer to the second element to compare (must point to a `double`).
 * @return   -1 if *a < *b, 1 if *a > *b, or 0 if equal.
 */
int compare_doubles(const void *a, const void *b)
{
	double da = *(const double *)a;
	double db = *(const double *)b;
	if (da < db)
	{
		return -1;
	}

	if (da > db)
	{
		return 1;
	}

	return 0;
}

/**
 * @brief Checks whether a parent process is still running.
 *
 * On Windows (`_WIN32`), opens a handle to the process with the given PID
 * and performs a non-blocking `WaitForSingleObject()` with zero timeout:
 * - Returns 1 if the wait times out (parent is still alive).
 * - Returns 0 if the object is signaled (parent has terminated).
 * - Returns -1 on error (failed to open handle or wait operation).
 *
 * On Linux (`__linux__`) and other POSIX systems, compares `getppid()` to
 * the expected `parent_pid`. When the original parent dies, the calling
 * process’s parent PID changes (typically to 1):
 * - Returns 1 if `getppid() == parent_pid` (parent alive).
 * - Returns 0 if `getppid() != parent_pid` (parent terminated).
 *
 * @param parent_pid  Process ID of the parent to check.
 * @return            1 if the parent is still alive, 0 if it has exited, -1 on error.
 */
int check_parent_status(const int parent_pid)
{
#ifdef _WIN32
	// Open a handle to the parent process with SYNCHRONIZE rights.
	HANDLE parent_handle = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)parent_pid);
	if (parent_handle == NULL)
	{
		ERROR_MESSAGE("Failed to open parent process handle. Error: %ld\n", GetLastError());
		return -1;
	}
	// Use a non-blocking wait.
	DWORD result = WaitForSingleObject(parent_handle, 0);
	CloseHandle(parent_handle);
	if (result == WAIT_TIMEOUT)
	{
		// Parent is still alive.
		return 1;
	}

	if (result == WAIT_OBJECT_0)
	{
		// Parent has terminated.
		return 0;
	}

	ERROR_MESSAGE("WaitForSingleObject failed: %ld\n", GetLastError());
	return -1;
#elifdef __linux__
	// Optionally, if you haven't already, you can call prctl(PR_SET_PDEATHSIG, SIGTERM)
	// earlier in your child process initialization.
	//
	// Check if the current parent process ID matches the expected parent PID.
	// When the parent dies, getppid() will change (usually to 1, the init process).
	if (getppid() == parent_pid)
	{
		return 1; // Parent is still alive.
	}

	return 0; // Parent has terminated.
#else
	// For other Unix-like systems, a similar getppid() check can be used.
	if (getppid() == parent_pid)
	{
		return 1;
	}

	return 0;
#endif
}

/**
 * @brief Checks the execution status of a child process without blocking.
 *
 * If `child_pid` ≤ 0, immediately returns `CHILD_ERROR_STATUS`. Otherwise:
 * - **Windows (_WIN32):**
 *   - Opens a handle to the process with `OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION)`.
 *   - Calls `WaitForSingleObject()` with zero timeout:
 *     - Returns `CHILD_STILL_RUNNING` if the wait times out (child is running).
 *     - If signaled, calls `GetExitCodeProcess()` and returns the exit code (0–255).
 *     - On error, logs via `log_message()` and returns `CHILD_ERROR_STATUS`.
 * - **POSIX:**
 *   - Calls `waitpid(child_pid, &status, WNOHANG)`:
 *     - If returns 0, child is still running → `CHILD_STILL_RUNNING`.
 *     - If returns `child_pid`, examines `status`:
 *       - `WIFEXITED` → return `WEXITSTATUS(status)` (0–255).
 *       - `WIFSIGNALED` → return `-WTERMSIG(status)` to indicate signal termination.
 *       - Otherwise → `CHILD_ERROR_STATUS`.
 *     - On error (`waitpid` < 0), logs via `log_message()` and returns `CHILD_ERROR_STATUS`.
 *
 * @param child_pid  Process ID of the child to check.
 * @return           `CHILD_STILL_RUNNING` if the child is alive;
 *                   child’s exit code (0–255) if it exited normally;
 *                   negative signal number if terminated by a signal;
 *                   `CHILD_ERROR_STATUS` on error or invalid PID.
 */
int check_duplicate_status_of_child(const int child_pid)
{
	if (child_pid <= 0)
	{
		return CHILD_ERROR_STATUS;
	}

#ifdef _WIN32
	DWORD status = 0;
	HANDLE child_handle = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, child_pid);
	if (child_handle == NULL)
	{
		log_message("Failed to open child process handle.\n");
		return CHILD_ERROR_STATUS;
	}

	// Non-blocking check of the child's status.
	DWORD result = WaitForSingleObject(child_handle, 0);
	if (result == WAIT_TIMEOUT)
	{
		// The child process is still running.
		CloseHandle(child_handle);
		return CHILD_STILL_RUNNING;
	}

	if (result == WAIT_OBJECT_0)
	{
		// The child process has terminated; get its exit status.
		if (GetExitCodeProcess(child_handle, &status))
		{
			CloseHandle(child_handle);
			return (int)status;
		}

		log_message("Failed to get exit code for child process.\n");
		CloseHandle(child_handle);
		return CHILD_ERROR_STATUS;
	}

	log_message("WaitForSingleObject failed with error: %ld\n", GetLastError());
	CloseHandle(child_handle);
	return CHILD_ERROR_STATUS;
#else
	int status = 0;
	pid_t result = waitpid(child_pid, &status, WNOHANG);
	if (result == 0)
	{
		// The child process is still running.
		return CHILD_STILL_RUNNING;
	}

	if (result == child_pid)
	{
		if (WIFEXITED(status))
		{
			// Return the child's exit code.
			return WEXITSTATUS(status);
		}

		if (WIFSIGNALED(status))
		{
			// Return a negative value to indicate termination by signal.
			return -WTERMSIG(status);
		}

		return CHILD_ERROR_STATUS;
	}

	log_message("waitpid failed for child_pid: %d, result: %d\n", child_pid, result);
	return CHILD_ERROR_STATUS;
#endif
}

/**
 * @brief CPU time usage counters for the current system.
 *
 * This structure holds cumulative CPU time metrics, varying by platform:
 *
 * - **Windows (_WIN32):**
 *   - `idle_time`   : Time spent idle.
 *   - `kernel_time` : Time spent in kernel mode.
 *   - `user_time`   : Time spent in user mode.
 *   All fields are `ULARGE_INTEGER` values as returned by `GetSystemTimes()`.
 *
 * - **macOS (__APPLE__):**
 *   - `user`   : Time spent executing user processes.
 *   - `system` : Time spent in kernel processes.
 *   - `nice`   : Time spent on low-priority (nice) processes.
 *   - `idle`   : Time spent idle.
 *   All fields are 64-bit ticks as returned by `host_processor_info()`.
 *
 * - **Linux (__linux__):**
 *   - `user`    : Time spent executing processes in user mode.
 *   - `nice`    : Time spent executing low-priority processes.
 *   - `system`  : Time spent executing processes in kernel mode.
 *   - `idle`    : Time spent idle.
 *   - `iowait`  : Time waiting for I/O to complete.
 *   - `irq`     : Time servicing hardware interrupts.
 *   - `softirq` : Time servicing software interrupts.
 *   - `steal`   : Time stolen by other operating systems in virtualized environments.
 *   All fields are `unsigned long long` jiffies read from `/proc/stat`.
 */
typedef struct cpu_times_t
{
#ifdef _WIN32
	ULARGE_INTEGER idle_time;
	ULARGE_INTEGER kernel_time;
	ULARGE_INTEGER user_time;
#elifdef __APPLE__
	uint64_t user;
	uint64_t system;
	uint64_t nice;
	uint64_t idle;
#elifdef __linux__
	unsigned long long user;
	unsigned long long nice;
	unsigned long long system;
	unsigned long long idle;
	unsigned long long iowait;
	unsigned long long irq;
	unsigned long long softirq;
	unsigned long long steal;
#endif
} cpu_times_t;

/**
 * @brief Retrieves cumulative CPU time statistics for the current system.
 *
 * Populates the provided `cpu_times_t` structure with platform-specific CPU time counters:
 *
 * - **Windows (_WIN32):**
 *   Uses `GetSystemTimes()` to fill `idle_time`, `kernel_time`, and `user_time` as `ULARGE_INTEGER`.
 * - **macOS (__APPLE__):**
 *   Calls `host_statistics(HOST_CPU_LOAD_INFO)` to fill `user`, `system`, `nice`, and `idle` tick counts.
 * - **Linux (__linux__):**
 *   Reads the first line of `/proc/stat` and parses jiffies for `user`, `nice`, `system`, `idle`,
 *   `iowait`, `irq`, `softirq`, and `steal`.
 *
 * @param times  Pointer to a `cpu_times_t` struct to be filled with CPU time data.
 */
#ifdef _WIN32
void get_cpu_times(cpu_times_t *times)
{
	FILETIME idle_time;
	FILETIME kernel_time;
	FILETIME user_time;
	if (GetSystemTimes(&idle_time, &kernel_time, &user_time))
	{
		times->idle_time.LowPart = idle_time.dwLowDateTime;
		times->idle_time.HighPart = idle_time.dwHighDateTime;
		times->kernel_time.LowPart = kernel_time.dwLowDateTime;
		times->kernel_time.HighPart = kernel_time.dwHighDateTime;
		times->user_time.LowPart = user_time.dwLowDateTime;
		times->user_time.HighPart = user_time.dwHighDateTime;
	}
	else
	{
		safe_fprintf(stderr, "GetSystemTimes failed.\n");
	}
}
#elifdef __APPLE__
void get_cpu_times(cpu_times_t *times)
{
	host_cpu_load_info_data_t cpuinfo = {0};
	mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
	if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpuinfo, &count) == KERN_SUCCESS)
	{
		times->user = cpuinfo.cpu_ticks[CPU_STATE_USER];
		times->system = cpuinfo.cpu_ticks[CPU_STATE_SYSTEM];
		times->nice = cpuinfo.cpu_ticks[CPU_STATE_NICE];
		times->idle = cpuinfo.cpu_ticks[CPU_STATE_IDLE];
	}
	else
	{
		safe_fprintf(stderr, "host_statistics failed.\n");
	}
}
#elifdef __linux__
void get_cpu_times(cpu_times_t *times)
{
	FILE *file = xflow_fopen_safe("/proc/stat", XFLOW_FILE_READ_ONLY);
	if (!file)
	{
		safe_fprintf(stderr, "Failed to open /proc/stat\n");
		return;
	}

	char buffer[256];
	if (fgets(buffer, sizeof(buffer), file))
	{
		char *saveptr = NULL;
		const char *field = NULL;

		/* skip the “cpu” label */
		(void)strtok_r(buffer, " \t\n", &saveptr);

		/* now parse each numeric field in turn */
		field = strtok_r(NULL, " \t\n", &saveptr);
		times->user = field ? strtoull(field, NULL, 10) : 0;

		field = strtok_r(NULL, " \t\n", &saveptr);
		times->nice = field ? strtoull(field, NULL, 10) : 0;

		field = strtok_r(NULL, " \t\n", &saveptr);
		times->system = field ? strtoull(field, NULL, 10) : 0;

		field = strtok_r(NULL, " \t\n", &saveptr);
		times->idle = field ? strtoull(field, NULL, 10) : 0;

		field = strtok_r(NULL, " \t\n", &saveptr);
		times->iowait = field ? strtoull(field, NULL, 10) : 0;

		field = strtok_r(NULL, " \t\n", &saveptr);
		times->irq = field ? strtoull(field, NULL, 10) : 0;

		field = strtok_r(NULL, " \t\n", &saveptr);
		times->softirq = field ? strtoull(field, NULL, 10) : 0;

		field = strtok_r(NULL, " \t\n", &saveptr);
		times->steal = field ? strtoull(field, NULL, 10) : 0;
	}

	if (fclose(file) == EOF)
	{
		ERROR_MESSAGE("Error closing %s: %s\n", "/proc/stat", safe_strerror(errno));
	}
}
#endif

/**
 * @brief Calculates CPU usage percentage between two sampling points.
 *
 * Computes the difference in idle and active times between `prev` and `curr`,
 * then returns the percentage of time spent on non-idle work over the interval.
 *
 * - **Windows (_WIN32):**
 *   - `idle_diff`   = Δ idle time
 *   - `kernel_diff` = Δ kernel time (includes idle)
 *   - `user_diff`   = Δ user time
 *   - `total_diff`  = `kernel_diff + user_diff`
 *   - CPU% = `(total_diff - idle_diff) / total_diff * 100`
 *
 * - **macOS (__APPLE__):**
 *   - `idle_diff`   = Δ idle ticks
 *   - `user_diff`   = Δ user ticks
 *   - `system_diff` = Δ system ticks
 *   - `nice_diff`   = Δ nice ticks
 *   - `total_diff`  = `idle_diff + user_diff + system_diff + nice_diff`
 *   - CPU% = `(user_diff + system_diff + nice_diff) / total_diff * 100`
 *
 * - **Linux (__linux__):**
 *   - `prev_idle`   = previous idle + iowait
 *   - `curr_idle`   = current idle + iowait
 *   - `prev_non_idle`= sum of previous user, nice, system, irq, softirq, steal
 *   - `curr_non_idle`= sum of current user, nice, system, irq, softirq, steal
 *   - `total_diff`  = `(curr_idle + curr_non_idle) - (prev_idle + prev_non_idle)`
 *   - `idle_diff`   = `curr_idle - prev_idle`
 *   - CPU% = `(total_diff - idle_diff) / total_diff * 100`
 *
 * @param prev  Pointer to `cpu_times_t` sampled at the start of the interval.
 * @param curr  Pointer to `cpu_times_t` sampled at the end of the interval.
 * @return      Percentage of CPU utilization (0.0–100.0) over the interval;
 *              returns 0.0 if no time difference is detected.
 */
#ifdef _WIN32
double calculate_cpu_usage(const cpu_times_t *prev, const cpu_times_t *curr)
{
	ULONGLONG idle_diff = curr->idle_time.QuadPart - prev->idle_time.QuadPart;
	ULONGLONG kernel_diff = curr->kernel_time.QuadPart - prev->kernel_time.QuadPart;
	ULONGLONG user_diff = curr->user_time.QuadPart - prev->user_time.QuadPart;
	ULONGLONG total_diff = kernel_diff + user_diff;
	if (total_diff == 0)
	{
		return 0.0;
	}
	// Note: kernel_diff includes idle time, so subtract idle_diff
	return ((double)(total_diff - idle_diff) * 100.0) / (double)total_diff;
}
#elifdef __APPLE__
double calculate_cpu_usage(const cpu_times_t *prev, const cpu_times_t *curr)
{
	uint64_t idle_diff = curr->idle - prev->idle;
	uint64_t user_diff = curr->user - prev->user;
	uint64_t system_diff = curr->system - prev->system;
	uint64_t nice_diff = curr->nice - prev->nice;
	uint64_t total_diff = idle_diff + user_diff + system_diff + nice_diff;
	if (total_diff == 0)
	{
		return 0.0;
	}
	return (double)(user_diff + system_diff + nice_diff) * 100.0 / (double)total_diff;
}
#elifdef __linux__
double calculate_cpu_usage(const cpu_times_t *prev, const cpu_times_t *curr)
{
	unsigned long long prev_idle = prev->idle + prev->iowait;
	unsigned long long curr_idle = curr->idle + curr->iowait;
	unsigned long long prev_non_idle = prev->user + prev->nice + prev->system + prev->irq + prev->softirq + prev->steal;
	unsigned long long curr_non_idle = curr->user + curr->nice + curr->system + curr->irq + curr->softirq + curr->steal;
	unsigned long long prev_total = prev_idle + prev_non_idle;
	unsigned long long curr_total = curr_idle + curr_non_idle;
	unsigned long long total_diff = curr_total - prev_total;
	unsigned long long idle_diff = curr_idle - prev_idle;
	if (total_diff == 0)
	{
		return 0.0;
	}
	return ((double)(total_diff - idle_diff) * 100.0) / (double)total_diff;
}
#endif

/**
 * @brief Computes CPU utilization since the last invocation.
 *
 * On the first call, initializes an internal snapshot of cumulative CPU times and
 * returns 0.0, as there is no prior data to compare. On subsequent calls, retrieves
 * the current CPU times, calculates the usage percentage over the interval using
 * `calculate_cpu_usage()`, updates the snapshot, and returns the computed value.
 *
 * @return CPU usage percentage (0.0–100.0) measured since the previous call;
 *         returns 0.0 on the very first call.
 *
 * @note
 * - Uses thread-local storage to retain the previous CPU times (`prev`) and
 *   an initialization flag across calls.
 * - Relies on `get_cpu_times()` to sample cumulative CPU counters and
 *   `calculate_cpu_usage()` to compute the delta percentage.
 * - Suitable for periodic invocation at arbitrary intervals.
 */
double update_cpu_usage(void)
{
	static _Thread_local cpu_times_t prev;
	static _Thread_local int initialized = 0;
	cpu_times_t curr;
	get_cpu_times(&curr);
	if (!initialized)
	{
		// On first call, just initialize the snapshot.
		prev = curr;
		initialized = 1;
		// Not enough data yet to compute usage.
		return 0.0;
	}

	double usage = calculate_cpu_usage(&prev, &curr);
	prev = curr;
	return usage;
}

/**
 * @brief Writes a CSV header row to a file under semaphore protection.
 *
 * Acquires the named semaphore before opening the specified file in write mode,
 * writes a header line starting with `"epoch_time"` followed by the null-terminated
 * list of column names in `headers[]`, then closes the file and releases the semaphore.
 * If any file I/O or semaphore operation fails, logs an error via `ERROR_MESSAGE()`
 * and sets `shutdownFlag` to trigger program termination.
 *
 * @param filename   Path to the CSV file where the header will be written.
 * @param sem_info   Pointer to a `semaphore_info_t` used to synchronize file access.
 * @param headers    Null-terminated array of column name strings to include after `epoch_time`.
 */
void save_csv_header(const char *filename, semaphore_info_t *sem_info, const char **headers)
{
	// Acquire the semaphore before writing to the file.
	if (shmem_wait_check(sem_info, "dp"))
	{
		shutdownFlag = 1;
	}

	// Use the unified library function to open the file for writing.
	FILE *file = xflow_fopen_safe(filename, XFLOW_FILE_WRITE_ONLY);
	if (file == NULL)
	{
		ERROR_MESSAGE("Failed to open file for writing: %s\n", filename);
		if (shmem_post_check(sem_info, "dp"))
		{
			shutdownFlag = 1;
		}
		return;
	}

	// Write the header row: first column is "epoch_time" followed by the provided header names.
	safe_fprintf(file, "epoch_time");
	int i = 0;
	while (headers[i] != NULL)
	{
		safe_fprintf(file, ",%s", headers[i]);
		i++;
	}
	safe_fprintf(file, "\n");

	if (fclose(file) == EOF)
	{
		ERROR_MESSAGE("Error closing %s: %s\n", filename, safe_strerror(errno));
	}

	// Release the semaphore after writing is complete.
	if (shmem_post_check(sem_info, "dp"))
	{
		shutdownFlag = 1;
	}
}

/**
 * @brief Appends a timestamped row of double values to a CSV file under semaphore protection.
 *
 * Acquires the specified semaphore before opening the CSV file in append mode,
 * writes the current real-time epoch timestamp (`seconds.nanoseconds`) followed by
 * the `n_data` double values as CSV columns, then closes the file and releases the semaphore.
 * If any file I/O or semaphore operation fails, logs an error via `ERROR_MESSAGE()`
 * and sets `shutdownFlag` to trigger shutdown.
 *
 * @param filename   Path to the CSV file where data will be appended.
 * @param sem_info   Pointer to a `semaphore_info_t` used to synchronize access to the file.
 * @param data       Array of `n_data` double values to write as CSV columns.
 * @param n_data     Number of elements in the `data` array.
 *
 * @note
 * - Uses `get_monotonic_timestamp()` to obtain the current time since boot.
 * - Formats the timestamp with `TIME_FORMAT` for seconds and `.%.5ld` for fractional seconds.
 * - Ensures mutual exclusion via `shmem_wait_check()` and `shmem_post_check()`.
 */
void save_double_array_data_to_csv(const char *filename, semaphore_info_t *sem_info, const double *data, int n_data)
{
	// Acquire the semaphore before writing to the file
	if (shmem_wait_check(sem_info, "dp"))
	{
		shutdownFlag = 1;
	}

	// Use the unified library function to open the file for appending.
	FILE *file = xflow_fopen_safe(filename, XFLOW_FILE_APPEND);
	if (file == NULL)
	{
		ERROR_MESSAGE("Failed to open file for writing: %s\n", filename);
		if (shmem_post_check(sem_info, "dp"))
		{
			shutdownFlag = 1;
		}
		return;
	}

	// Get the current timestamp
	struct timespec ts = get_monotonic_timestamp();

	// Write the data row: first column is epoch time (seconds.nanoseconds)
	safe_fprintf(file, TIME_FORMAT ".%.5ld", ts.tv_sec, ts.tv_nsec);

	// Write each double value into its corresponding column
	for (int i = 0; i < n_data; i++)
	{
		safe_fprintf(file, ",%.10f", data[i]);
	}
	safe_fprintf(file, "\n");

	// Close the file
	if (fclose(file) == EOF)
	{
		ERROR_MESSAGE("Error closing %s: %s\n", filename, safe_strerror(errno));
	}

	// Release the semaphore after writing is complete
	if (shmem_post_check(sem_info, "dp"))
	{
		shutdownFlag = 1;
	}
}
