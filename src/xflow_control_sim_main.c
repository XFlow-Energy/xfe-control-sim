/**
 * @file    xflow_control_sim_main.c
 * @author  XFlow Energy
 * @brief   Contains the main function for the XFE Control Simulation software.
 */

/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * XFLOW-CONTROL-SIM
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

#include "control_switch.h"           // for control_switch
#include "data_processing.h"          // for data_processing, BEGINNING
#include "flow_gen.h"                 // for flow_gen
#include "logger.h"                   // for log_message, ERROR_MESSAGE
#include "maybe_unused.h"             // for MAYBE_UNUSED
#include "numerical_integrator.h"     // for numerical_integrator
#include "turbine_controls.h"         // for turbine_control
#include "xflow_aero_sim.h"             // for get_param, update_csv_value
#include "xflow_control_sim_common.h" // for continuous_logging_function
#include "xflow_control_sim_version.h"
#include "xflow_core.h"                 // for get_monotonic_timestamp, clo...
#include "xflow_modbus_server_client.h" // for childPID
#include <errno.h>                      // for errno
#include <modbus/modbus.h>              // for ON
#include <stdlib.h>                     // for NULL
#include <string.h>                     // for strerror, strcmp

#ifdef __APPLE__
#include <sys/types.h> // for pid_t
#include <time.h>      // for timespec
#endif

#ifdef _WIN32
// NOLINTBEGIN(llvm-include-order)
#include <winsock2.h>
#include <windows.h>
// NOLINTEND(llvm-include-order)
#else
#include <signal.h>   // For kill, SIGTERM, SIGKILL
#include <sys/wait.h> // For waitpid, WIFEXITED, WEXITSTATUS, etc.
#endif

void end_modbus_server(void)
{
	// log_message("in end_modbus_server\n");
	if (childPID <= 0)
	{
		// log_message("Invalid PID: %d. No modbus_server process to terminate.\n", childPID);
		return;
	}

#ifdef _WIN32
	HANDLE process_handle = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, childPID);
	if (process_handle == NULL)
	{
		ERROR_MESSAGE("Failed to open modbus_server process (PID %d): %lu\n", childPID, GetLastError());
		return;
	}

	// Try to gracefully terminate the process
	if (TerminateProcess(process_handle, 0))
	{
		log_message("Sent termination request to modbus_server (PID %d).\n", childPID);
	}
	else
	{
		ERROR_MESSAGE("Failed to terminate modbus_server (PID %d): %lu\n", childPID, GetLastError());
		CloseHandle(process_handle);
		return;
	}

	// Wait for the process to exit
	DWORD wait_result = WaitForSingleObject(process_handle, 5000); // Wait for up to 5 seconds
	if (wait_result == WAIT_OBJECT_0)
	{
		DWORD exit_code = 0;
		if (GetExitCodeProcess(process_handle, &exit_code))
		{
			if (exit_code == 0)
			{
				log_message("modbus_server (PID %d) exited with status %lu.\n", childPID, exit_code);
			}
			else
			{
				log_message("modbus_server (PID %d) exited with non-zero status %lu.\n", childPID, exit_code);
			}
		}
		else
		{
			ERROR_MESSAGE("Failed to get exit code for modbus_server (PID %d): %lu\n", childPID, GetLastError());
		}
	}
	else if (wait_result == WAIT_TIMEOUT)
	{
		log_message("modbus_server (PID %d) did not exit in time. Forcibly terminating.\n", childPID);
		TerminateProcess(process_handle, 1); // Forcefully terminate the process
	}
	else
	{
		ERROR_MESSAGE("Failed to wait for modbus_server (PID %d): %lu\n", childPID, GetLastError());
	}

	CloseHandle(process_handle);

#else
	// POSIX-specific process termination and wait logic
	if (kill(childPID, SIGTERM) == 0)
	{
		log_message("Sent SIGTERM to modbus_server (PID %d).\n", childPID);
	}
	else
	{
		ERROR_MESSAGE("Failed to send SIGTERM to modbus_server (PID %d): %s\n", childPID, safe_strerror(errno));
		return;
	}

	int status = 0;
	pid_t result = waitpid(childPID, &status, 0);
	if (result == -1)
	{
		ERROR_MESSAGE("Failed to wait for modbus_server (PID %d): %s\n", childPID, safe_strerror(errno));
	}
	else if (WIFEXITED(status))
	{
		log_message("modbus_server (PID %d) exited with status %d.\n", childPID, WEXITSTATUS(status));
	}
	else if (WIFSIGNALED(status))
	{
		log_message("modbus_server (PID %d) was terminated by signal %d.\n", childPID, WTERMSIG(status));
	}
	else
	{
		log_message("modbus_server (PID %d) exited unexpectedly.\n", childPID);
	}

	// If the process is still running, send SIGKILL to forcefully terminate it
	if (kill(childPID, 0) == 0)
	{
		log_message("modbus_server (PID %d) did not exit after SIGTERM. Sending SIGKILL.\n", childPID);
		if (kill(childPID, SIGKILL) == 0)
		{
			log_message("Sent SIGKILL to modbus_server (PID %d).\n", childPID);
		}
		else
		{
			ERROR_MESSAGE("Failed to send SIGKILL to modbus_server (PID %d): %s\n", childPID, safe_strerror(errno));
		}
	}
#endif
}

void cleanup_program(MAYBE_UNUSED int signum)
{
	end_modbus_server();
	// fflush(stderr);
}

int main(const int argc, const char *argv[])
{
	const struct timespec time_beg = get_monotonic_timestamp();
	static param_array_t *dynamic_Data = NULL;
	static param_array_t *fixed_Data = NULL;

	int logging_status = 1;
	long parent_pid_initial = 0;
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--logging") == 0)
		{
			logging_status = safe_atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "--parentpid") == 0)
		{
			parent_pid_initial = safe_atoi(argv[++i]);
		}
	}
	// Initialize the signal handler to catch signals which will stop the program.
	initialize_signal_handler();

	int run_single_mode_only = 0;

#ifdef RUN_SINGLE_MODEL_ONLY
	if (RUN_SINGLE_MODEL_ONLY)
	{
		int run_single_one = 1;
		update_csv_value(SYSTEM_CONFIG_FULL_PATH, "data_processing_single_run_only", INPUT_PARAM_INT, &run_single_one);
		run_single_mode_only = 1;
	}
	else
	{
		int run_single_zero = 0;
		update_csv_value(SYSTEM_CONFIG_FULL_PATH, "data_processing_single_run_only", INPUT_PARAM_INT, &run_single_zero);
		run_single_mode_only = 0;
	}
#endif

	// Pass the address of the pointers (i.e., pointers to pointers)
	initialize_control_system(&dynamic_Data, &fixed_Data, logging_status != 0);
	log_message("xflow-control-sim git commit info: %s\n", gitCommitInfoXfeControlSim);

	static double **state_Vars = NULL; // Array of pointers to .value.d fields
	static const char **state_Names = NULL;

	// Get what variables are state variables for the numerical integrator
	const int num_state_vars = init_state_bindings(dynamic_Data, &state_Vars, &state_Names);

	// log_message("parent_pid: %ld\n", parent_pid_initial);

	static double *dt_Sec = NULL;
	static double *dur_Sec = NULL;
	static double *time_Sec = NULL;
	static double *control_Dt_Sec = NULL;
	static int *enable_Brake_Signal = NULL;
	static double *omega = NULL;

	// below used only for data processing or optimization.
	static int *data_Processing_Status = NULL;
	static int *data_Processing_First_Run = NULL;

	static int *parent_Pid = NULL;

	get_param(fixed_Data, "dt_sec", &dt_Sec);
	get_param(fixed_Data, "dur_sec", &dur_Sec);
	get_param(dynamic_Data, "time_sec", &time_Sec);
	get_param(fixed_Data, "control_dt_sec", &control_Dt_Sec);
	get_param(dynamic_Data, "enable_brake_signal", &enable_Brake_Signal);
	get_param(dynamic_Data, "omega", &omega);

	get_param(dynamic_Data, "data_processing_status", &data_Processing_Status);
	get_param(fixed_Data, "data_processing_first_run", &data_Processing_First_Run);

	get_param(dynamic_Data, "parent_pid", &parent_Pid);
	*parent_Pid = (int)parent_pid_initial;

	// log_message("argv[0]: %s\n", argv[0]);

	update_csv_value(SYSTEM_CONFIG_FULL_PATH, "program_name", INPUT_PARAM_STRING, (void *)argv[0]);
	update_csv_value(SYSTEM_CONFIG_FULL_PATH, "program_argc", INPUT_PARAM_INT, (void *)&argc);

	control_switch(dynamic_Data, fixed_Data);

#ifdef BUILD_XFE_SCADA_INTERFACE
	log_message("running BUILD_XFE_SCADA_INTERFACE\n");
	const struct timespec programStartTime = get_monotonic_timestamp();
	while (*time_Sec < *dur_Sec && !shutdownFlag)
	{
		const struct timespec while_loop_start_time = get_monotonic_timestamp();
		flow_gen(dynamic_Data, fixed_Data);
		numerical_integrator(state_Vars, state_Names, num_state_vars, *dt_Sec, dynamic_Data, fixed_Data);
		*time_Sec = timespec_diff_to_double(programStartTime, get_monotonic_timestamp());
		turbine_control(dynamic_Data, fixed_Data);

		continuous_logging_function(dynamic_Data, fixed_Data);

		const double while_loop_duration_time = timespec_diff_to_double(while_loop_start_time, get_monotonic_timestamp());
		log_message("while_loop_duration_time %f, *time_Sec: %f\n", while_loop_duration_time, *time_Sec);
		const double sleep_time = *dt_Sec - while_loop_duration_time;
		if (sleep_time <= 0)
		{
			log_message("sleep_time less than 0: %f\n", sleep_time);
			continue;
		}
		else
		{
			const uint32_t sleep_time_command = (1e6 * sleep_time);
			// log_message("sleep_time_command: %d\n", sleep_time_command);
			usleep_now(sleep_time_command);
		}
	}
#else

	// Populate the program_args struct.
	data_processing_program_args_t dp_options = {
		.argc = argc,
		.argv = argv
	};

	// have new function here that checks here where we see if its the first run or not.
	// if it is then we need to open csv file where the new data will be stored
	// and the semephore for opening up the csv file.
	*data_Processing_Status = BEGINNING;
	flow_gen(dynamic_Data, fixed_Data); // call in the beginning to load in the flow time series.
	data_processing(dynamic_Data, fixed_Data, &dp_options);
	*data_Processing_Status = LOOPING;

	static double accumulated_Time = 0.0;
	// log_message("running normal simulation, *time_Sec: %f, *dur_Sec: %f\n", *time_Sec, *dur_Sec);
	while (*time_Sec < *dur_Sec && !shutdownFlag && (!*data_Processing_First_Run || run_single_mode_only))
	{
		flow_gen(dynamic_Data, fixed_Data);

		numerical_integrator(state_Vars, state_Names, num_state_vars, *dt_Sec, dynamic_Data, fixed_Data);
		if (*enable_Brake_Signal != 0 && *omega < 0.5)
		{
			*omega = 0;
		}
		*time_Sec += *dt_Sec;
		// Add the elapsed time since the last update
		accumulated_Time += *dt_Sec;

		// Check if the accumulated time has reached or exceeded control_dt_sec
		if (accumulated_Time >= *control_Dt_Sec)
		{
			// call the turbine control every control_dt_sec timestep.
			turbine_control(dynamic_Data, fixed_Data); // update the vfd torque command
			accumulated_Time -= *control_Dt_Sec;       // Reset accumulated_Time, preserve any leftover time
		}

		continuous_logging_function(dynamic_Data, fixed_Data);

		data_processing(dynamic_Data, fixed_Data, &dp_options); // If applicable track the requested data for processing at end of run.
	}

	*data_Processing_Status = ENDING;
	data_processing(dynamic_Data, fixed_Data, &dp_options); // while loop has ended so its time to complete last steps of the data processing.
#endif

	const struct timespec program_duration = timespec_diff(time_beg, get_monotonic_timestamp());
	log_message("Program Duration: %ld.%.5ld\n", program_duration.tv_sec, program_duration.tv_nsec / 10000);

#if defined(LOGGING_DYNAMIC_DATA_CONTINUOUS) && defined(DYNAMIC_DATA_FULL_PATH)
	// save_param_array_data_to_csv(DYNAMIC_DATA_FULL_PATH, dynamic_Data, 0);
	dynamic_data_csv_logger(CSV_LOGGER_CLOSE, DYNAMIC_DATA_FULL_PATH, dynamic_Data);
#endif

	// Graceful shutdown code
	if (shutdownFlag)
	{
		// log_message("Shutdown signal received. Cleaning up...\n");
		save_dynamic_fixed_data_at_shutdown(dynamic_Data, fixed_Data, logging_status != 0);

		close_log_file();
		cleanup_program(0);

		// Step 5: Free the input data memory
		free_input_data(dynamic_Data);
		free_input_data(fixed_Data);
	}

	cleanup_program(0);
	log_message("Closing Program\n");

	return 0;
}
