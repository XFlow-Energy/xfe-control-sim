/**
 * @file    modbus_server.c
 * @author  XFlow Energy
 * @date    2025
 * @brief   Modbus server functions for interfacing with XFE-SCADA
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

#ifdef _WIN32
#include <winsock2.h> // For network-related functions (include before windows.h)
#include <windows.h>  // For MAX_PATH and VirtualFree
#else
#include <sys/mman.h> // For munmap on POSIX
#include <unistd.h>   // For close on POSIX
#endif

#include <stdlib.h>    // For NULL, exit, free, EXIT_FAILURE
#include <string.h>    // For strcmp
#include <limits.h>    // for PATH_MAX
#include <semaphore.h> // For sem_close on POSIX

#include "logger.h" // For log_message, ERROR_MESSAGE
#include "maybe_unused.h"
#include "modbus/modbus.h"              // For modbus_close, modbus_free
#include "xflow_core.h"                 // For initialize_signal_handler
#include "xflow_modbus.h"               // For COILS, modbus_type_index_t
#include "xflow_modbus_server_client.h" // For modbusDevices, modbusContext

#ifndef DELETE_LOG_FILE_NEW_RUN
#define DELETE_LOG_FILE_NEW_RUN 0
#endif

/**
 * @brief Performs cleanup of resources before program termination.
 *
 * Depending on the global `programType`, this function releases all allocated
 * sockets, modbus mappings, shared memory segments, semaphores, and CSV data:
 *
 * - **HARDWARE_CONNECTIONS**:
 *   - Closes the listening `serverSocket`.
 *   - For each active bus device:
 *     - Frees the `modbus_mapping_t`.
 *     - Frees and unmaps each shared-memory buffer (`shm_info.ptr`).
 *     - Closes each semaphore (`sem_info.ptr`).
 *   - If `modbusContext` is open and `modbusStatus == 1`, calls `modbus_close()` and `modbus_free()`.
 *
 * - **DATA_MANIPULATION**:
 *   - For all possible modbus mappings:
 *     - Frees and unmaps shared-memory buffers.
 *     - Closes semaphores.
 *
 * After releasing these resources, calls `free_csv_data()` to release CSV configuration
 * memory, logs completion, and exits the process with `EXIT_SUCCESS`. If `programType`
 * is unrecognized, logs an error and exits with `EXIT_FAILURE`.
 *
 * @param signum  (Unused) Signal number that triggered cleanup, marked `MAYBE_UNUSED`.
 */
void cleanup_program(MAYBE_UNUSED int signum)
{
	switch (programType)
	{
	case HARDWARE_CONNECTIONS:
		if (serverSocket != -1)
		{
#ifdef _WIN32
			closesocket(serverSocket);
#else
			close(serverSocket);
#endif
		}

		for (int i = 0; i < numActiveBusDevices; i++)
		{
			modbus_mapping_t *modbus_mapping = modbusDevices[i].modbus_mapping;
			if (modbus_mapping != NULL)
			{
				modbus_mapping_free(modbus_mapping);
			}

			for (modbus_type_index_t mb_idx = COILS; mb_idx < NUMBER_MODBUS_TYPES; mb_idx++)
			{
				free(modbusDevices[i].shm_sem_info[mb_idx].values);
				if (modbusDevices[i].shm_sem_info[mb_idx].shm_info.ptr != NULL)
				{
#ifdef _WIN32
					// Use VirtualFree to unmap memory on Windows
					VirtualFree(modbusDevices[i].shm_sem_info[mb_idx].shm_info.ptr, 0, MEM_RELEASE);
#else
					// Use munmap to unmap memory on POSIX
					munmap(modbusDevices[i].shm_sem_info[mb_idx].shm_info.ptr, modbusDevices[i].shm_sem_info[mb_idx].shm_info.size);
#endif
				}
				sem_close(modbusDevices[i].shm_sem_info[mb_idx].sem_info.ptr);
			}
		}

		if (modbusContext != NULL && modbusStatus == 1)
		{
			modbus_close(modbusContext);
			modbus_free(modbusContext);
		}
		break;

	case DATA_MANIPULATION:
		for (int i = 0; i < MAX_MODBUS_MAPPINGS; i++)
		{
			for (modbus_type_index_t mb_idx = COILS; mb_idx < NUMBER_MODBUS_TYPES; mb_idx++)
			{
				free(modbusDevices[i].shm_sem_info[mb_idx].values);
				if (modbusDevices[i].shm_sem_info[mb_idx].shm_info.ptr != NULL)
				{
#ifdef _WIN32
					VirtualFree(modbusDevices[i].shm_sem_info[mb_idx].shm_info.ptr, 0, MEM_RELEASE);
#else
					munmap(modbusDevices[i].shm_sem_info[mb_idx].shm_info.ptr, modbusDevices[i].shm_sem_info[mb_idx].shm_info.size);
#endif
				}
				sem_close(modbusDevices[i].shm_sem_info[mb_idx].sem_info.ptr);
			}
		}
		break;

	default:
		ERROR_MESSAGE("Invalid programType!\n");
		exit(EXIT_FAILURE);
	}

	// Call to free the allocated CSV data
	free_csv_data(csvConfigData, numActiveBusDevices, MAX_COLUMN_SIZE);

	log_message("cleanup finished\n");
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	// for (int i = 0; i < argc; i++)
	// {
	// 	log_message("argv[%d] %s\n", i, argv[i]);
	// }
	log_message("Starting modbus server, OUTPUT_LOG_FILE_PATH: %s\n", OUTPUT_LOG_FILE_PATH);

#ifdef OUTPUT_LOG_FILE_PATH
	char output_log_filename_aero[PATH_MAX];
	create_dynamic_file_path(output_log_filename_aero, sizeof(output_log_filename_aero), "%s", "modbus_server.log");
	char logfilename[PATH_MAX];
	log_file_ammend_remove_t log_ammend_delete;
#if DELETE_LOG_FILE_NEW_RUN == 1
	log_ammend_delete = DELETE_OLD_LOG_FILE;
#else
	log_ammend_delete = AMMEND_LOG_FILE;
#endif
	initialize_log_file(logfilename, PATH_MAX, OUTPUT_LOG_FILE_PATH, output_log_filename_aero, log_ammend_delete);
#endif

	initialize_signal_handler();

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--device_config_csv_file") == 0)
		{
			deviceConfigCSVFile = argv[++i];
		}
		else if (strcmp(argv[i], "--csv_file_location") == 0)
		{
			csvFileLocation = argv[++i];
		}
		else if (strcmp(argv[i], "--dev_num") == 0)
		{
			deviceNumber = safe_atoi(argv[++i]);
		}
	}

	// log_message("Program Name: %s\n", argv[0]);
	shutdownFlag = set_config_data(HARDWARE_CONNECTIONS);

	// for (int i = 0; i < MAX_MODBUS_MAPPINGS; i++)
	// {
	// 	for (modbus_type_index_t mb_idx = COILS; mb_idx < NUMBER_MODBUS_TYPES; mb_idx++)
	// 	{
	// 		// Print the memory location of shm_info.ptr
	// 		log_message("Memory location of modbusDevices[%d].shm_sem_info[%d].shm_info.ptr: %p\n", i, mb_idx, modbusDevices[i].shm_sem_info[mb_idx].shm_info.ptr);
	// 	}
	// }

	run_hardware_interface_program_logic(argc, argv);

	cleanup_program(0);
	log_message("Closing Program\n");
	return 0;
}
