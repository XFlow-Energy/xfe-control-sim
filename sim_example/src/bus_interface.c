/**
 * @file    bus_interface.c
 * @author  XFlow Energy
 * @date    2025
 * @brief   Turbine control for BUS Interface
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

#include <errno.h>   // for errno
#include <stdbool.h> // IWYU pragma: keep
#include <stdint.h>  // for uint16_t
#include <stdio.h>   // for snprintf
#include <stdlib.h>  // for exit, EXIT_FAILURE
#include <string.h>  // for strerror
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <io.h>
#else
#include <unistd.h> // for dup2, close, execve, fork
#endif
#include "xfe_control_sim_common.h"     // for get_param, param_array_t
#include "logger.h"                     // for log_message, ERROR_MESSAGE
#include "turbine_controls.h"           // for turbine_control
#include "xflow_core.h"                 // for create_dynamic_file_path
#include "xflow_data_types.h"           // for DATA_TYPE_INT16
#include "xflow_modbus.h"               // for COILS, convert_modbus_value
#include "xflow_modbus_server_client.h" // for childPID, modbusDevices, che...
#include "xflow_shmem_sem.h"            // for shared_memory_info_t, shmem_...
#include "xflow_time.h"
#include "bus_interface.h"
#ifndef _WIN32
#include <fcntl.h>    // for O_CREAT, O_RDWR
#include <sys/mman.h> // for shm_open, mmap
#include <math.h>     // for M_PI

// Simulation sensor data structure (shared with bus controller backends)
#define SIM_SENSOR_DATA_SHM_NAME "/sim_sensor_data"
#define SIM_SENSOR_TYPE "sim_sensor"
#define MAX_GPIO_PINS 50
#define MAX_PRU_CHANNELS 4

typedef struct
{
	int gpio_pins[MAX_GPIO_PINS]; // GPIO pin states (0.0 = OFF, 1.0 = ON)
	int coil_pins[MAX_GPIO_PINS];
	double pru_freq_hz[MAX_PRU_CHANNELS]; // PRU frequencies in Hz
	uint64_t last_update_us;              // Timestamp
} sim_sensor_data_t;
#endif

typedef enum
{
	GPIO_OFF = 11,
	GPIO_ON = 22
} gpio_off_on_t;

// Network configuration column indices for modbus_networks.csv
typedef enum
{
	NETWORK_ID_IDX = 0,
	IP_ADDRESS_IDX,
	TCP_PORT_IDX,
	TIMEOUT_US_IDX,
	DEVICE_CSV_FILENAME_IDX,
	MAX_NETWORK_COLUMNS
} network_config_index_t;

void launch_shared_mem_and_hardware_interface(void)
{
	char *modbus_server_executable_location_program_name = NULL;
#ifdef MODBUS_SERVER_EXECUTABLE_FULL_PATH
	modbus_server_executable_location_program_name = MODBUS_SERVER_EXECUTABLE_FULL_PATH;
#else
	ERROR_MESSAGE("MODBUS_SERVER_EXECUTABLE_FULL_PATH is not defined. Exiting...\n");
	exit(1);
#endif

#ifdef MODBUS_DEVICE_FULL_PATH
	csvFileLocation = MODBUS_DEVICE_FULL_PATH;
#else
	ERROR_MESSAGE("MODBUS_DEVICE_FULL_PATH is not defined. Exiting...\n");
	exit(1);
#endif

	// Read network configuration
	char ***networks_config_data = NULL;
	int num_networks = 0;

#ifdef MODBUS_NETWORKS_CONFIG_FULL_PATH
	networksConfigCSVFile = MODBUS_NETWORKS_CONFIG_FULL_PATH;
	networks_config_data = read_csv_char(networksConfigCSVFile, &num_networks);
	if (networks_config_data == NULL || num_networks == 0)
	{
		ERROR_MESSAGE("Failed to read networks configuration from %s\n", networksConfigCSVFile);
		exit(1);
	}
	if (num_networks > MAX_NETWORKS)
	{
		ERROR_MESSAGE("Too many networks (%d) configured. Maximum is %d\n", num_networks, MAX_NETWORKS);
		exit(1);
	}
	numActiveNetworks = num_networks;
	log_message("%d network(s) configured\n", num_networks);
#else
	ERROR_MESSAGE("MODBUS_NETWORKS_CONFIG_FULL_PATH is not defined. Exiting...\n");
	exit(1);
#endif

	// Configure devices for each network
	for (int net_idx = 0; net_idx < num_networks; net_idx++)
	{
		int network_id = safe_atoi(networks_config_data[net_idx][NETWORK_ID_IDX]);
		char device_csv_filename[PATH_MAX];
		create_dynamic_file_path(device_csv_filename, sizeof(device_csv_filename), "%s/%s", csvFileLocation, networks_config_data[net_idx][DEVICE_CSV_FILENAME_IDX]);

		deviceConfigCSVFile = device_csv_filename;
		deviceNumber = network_id;

		log_message("Network %d: Loading devices from %s\n", network_id, device_csv_filename);
		shutdownFlag = set_config_data(DATA_MANIPULATION);
		if (shutdownFlag != 0)
		{
			ERROR_MESSAGE("Failed to configure devices for network %d\n", network_id);
			exit(1);
		}
	}

	// Load all accumulated devices into shared memory
	load_data_into_shmem();
	log_message("after load_data_into_shmem\n");

	// Create simulation sensor data shared memory for GPIO/PRU simulation
#ifndef _WIN32
	create_simulation_sensor_data_shmem();
#endif

	// Launch modbus_server child process(es)
	for (int net_idx = 0; net_idx < num_networks; net_idx++)
	{
		int network_id = safe_atoi(networks_config_data[net_idx][NETWORK_ID_IDX]);
		char *ip_address = networks_config_data[net_idx][IP_ADDRESS_IDX];
		char *tcp_port = networks_config_data[net_idx][TCP_PORT_IDX];
		char *timeout_us = networks_config_data[net_idx][TIMEOUT_US_IDX];
		char device_csv_filename[PATH_MAX];
		create_dynamic_file_path(device_csv_filename, sizeof(device_csv_filename), "%s/%s", csvFileLocation, networks_config_data[net_idx][DEVICE_CSV_FILENAME_IDX]);

		char network_id_str[32];
		safe_snprintf(network_id_str, sizeof(network_id_str), "%d", network_id);

		log_message("Launching modbus_server for network %d (IP: %s, Port: %s)\n", network_id, ip_address, tcp_port);

#ifdef _WIN32
		pid_t pid = launch_modbus_server_windows(modbus_server_executable_location_program_name, network_id_str, device_csv_filename, ip_address, tcp_port, timeout_us);
#else
		pid_t pid = launch_modbus_server_unix(modbus_server_executable_location_program_name, network_id_str, device_csv_filename, ip_address, tcp_port, timeout_us);
#endif
		childPIDs[net_idx] = pid;
		log_message("Network %d: modbus_server PID %d\n", network_id, pid);
	}

	// Store first PID in legacy childPID for backward compatibility
	if (num_networks > 0)
	{
		childPID = childPIDs[0];
	}

	// Free networks config data
	free_csv_data(networks_config_data, num_networks, MAX_NETWORK_COLUMNS);
}

#ifdef _WIN32
// Platform-specific implementation for Windows
pid_t launch_modbus_server_windows(char *modbus_server_executable_location_program_name, char *network_id_str, char *device_csv_file, char *server_ip, char *tcp_port, char *timeout_us)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	safe_memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	safe_memset(&pi, 0, sizeof(pi));

	// Create the command line string with network-specific parameters
	char command_line[2048];
	safe_snprintf(command_line, sizeof(command_line), "\"%s\" --modbus_device_type \"%s\" --dev_num \"%s\" --device_config_csv_file \"%s\" --csv_file_location \"%s\" --server_ip \"%s\" --tcp_port \"%s\" --timeout_us \"%s\" --network_id \"%s\"", modbus_server_executable_location_program_name, MODBUS_DEVICE_TYPE, network_id_str, device_csv_file, csvFileLocation, server_ip, tcp_port, timeout_us, network_id_str);

	log_message("launch child command: %s\n", command_line);

#ifdef OUTPUT_LOG_FILE_PATH
	// Create the child process in new process group to prevent receiving Ctrl+C signals
	// CREATE_NEW_PROCESS_GROUP is Windows equivalent of Unix setpgid(0, 0)
	if (!CreateProcess(NULL, command_line, NULL, NULL, FALSE, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi))
#else
	// Use CREATE_NEW_CONSOLE and CREATE_NEW_PROCESS_GROUP to launch the child process
	if (!CreateProcess(
			NULL,                                          // No module name (use command line)
			command_line,                                  // Command line
			NULL,                                          // Process handle not inheritable
			NULL,                                          // Thread handle not inheritable
			FALSE,                                         // Set handle inheritance to FALSE
			CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP, // New console + new process group
			NULL,                                          // Use parent's environment block
			NULL,                                          // Use parent's starting directory
			&si,                                           // Pointer to STARTUPINFO structure
			&pi
		) // Pointer to PROCESS_INFORMATION structure
	)
#endif
	{
		DWORD error = GetLastError();
		ERROR_MESSAGE("CreateProcess failed with error: %lu\n", error);
		exit(1);
	}

	pid_t pid = pi.dwProcessId;
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	log_message("modbus_server started successfully with PID %d.\n", pid);
	Sleep(2000); // Sleep for 2000 milliseconds (2 seconds).
	return pid;
}
#else
// Platform-specific implementation for Unix-like systems (Linux/macOS)
pid_t launch_modbus_server_unix(char *modbus_server_executable_location_program_name, char *network_id_str, char *device_csv_file, char *server_ip, char *tcp_port, char *timeout_us)
{
	pid_t pid = fork();

	if (pid == 0) // Child process
	{
		// Put child in its own process group to prevent receiving terminal signals (SIGINT from Ctrl+C)
		// This allows parent to have full control over child lifecycle
		setpgid(0, 0);

		char *argv[] = {
			modbus_server_executable_location_program_name,
			"--modbus_device_type", MODBUS_DEVICE_TYPE,
			"--dev_num", network_id_str,
			"--device_config_csv_file", device_csv_file,
			"--csv_file_location", csvFileLocation,
			"--server_ip", server_ip,
			"--tcp_port", tcp_port,
			"--timeout_us", timeout_us,
			"--network_id", network_id_str,
			NULL
		};
		char *envp[] = {NULL};

		execve(modbus_server_executable_location_program_name, argv, envp);
		ERROR_MESSAGE("Failed to launch modbus_server: %s\n", safe_strerror(errno));
		exit(1);
	}
	else if (pid > 0)
	{
		log_message("modbus_server started successfully with PID %d.\n", pid);
		usleep_now(2000000);
		return pid;
	}
	else
	{
		ERROR_MESSAGE("Failed to fork: %s\n", safe_strerror(errno));
		exit(1);
	}
}
#endif

void modify_coil_information(const param_array_t *dynamic_data, const param_array_t *fixed_data, shared_memory_info_t *shm_reg_info, const int server_id, const int network_id)
{
	// Dispatch to device-specific logic based on (network_id, server_id)
	if (network_id == 4 && server_id == 1)
	{
		// Network 4, Server 1: Solis Inverter
		modify_solis_coils(dynamic_data, fixed_data, shm_reg_info);
	}
	// Add other device logic here as needed
}

void modify_input_coil_information(MAYBE_UNUSED const param_array_t *dynamic_data, MAYBE_UNUSED const param_array_t *fixed_data, MAYBE_UNUSED shared_memory_info_t *shm_reg_info, MAYBE_UNUSED const int server_id, MAYBE_UNUSED const int network_id)
{
}

void modify_holding_register_information(MAYBE_UNUSED const param_array_t *dynamic_data, MAYBE_UNUSED const param_array_t *fixed_data, MAYBE_UNUSED shared_memory_info_t *shm_reg_info, MAYBE_UNUSED const int server_id, MAYBE_UNUSED const int network_id)
{
	// log_message("Network %d, Server %d\n", network_id, server_id);
	if (network_id == 2 && server_id == 1)
	{
		modify_abb_vfd_holding_registers(dynamic_data, fixed_data, shm_reg_info);
	}
	// Network 4, Server 1: Solis Inverter
	else if (network_id == 4 && server_id == 1)
	{
		modify_solis_holding_registers(dynamic_data, fixed_data, shm_reg_info);
	}
}

// Solis Inverter-Specific Functions (Network 4, Server 1)

void modify_solis_holding_registers(const param_array_t *dynamic_data, MAYBE_UNUSED const param_array_t *fixed_data, shared_memory_info_t *shm_reg_info)
{
	// Static variables for holding register control values
	static double *inverter_Power_Command = NULL;
	static int *inverter_Enable_Signal = NULL;

	static bool first_Run = false;
	if (!first_Run)
	{
		// Initialize pointers to simulation data
		get_param(dynamic_data, "inverter_power_command", &inverter_Power_Command);
		get_param(dynamic_data, "inverter_enable_signal", &inverter_Enable_Signal);
		first_Run = true;
	}

	// SOLIS_INV_ONOFF (address 3007) - Inverter On/Off command
	// Value: 1 = On, 190 = Off
	if (inverter_Enable_Signal != NULL)
	{
		uint16_t onoff_value = (*inverter_Enable_Signal) ? 1 : 190;
		shm_reg_info->ptr[SOLIS_INV_ONOFF - 1] = onoff_value;
		// log_message("Solis Inverter: ONOFF command = %d (%s)\n", onoff_value, (*inverter_Enable_Signal) ? "ON" : "OFF");
	}

	// SOLIS_INV_OUTPUT_POWER_CONTROL (address 3052) - Output power control
	// Value in %kW, scaled by 100 (e.g., 10000 = 100.00 %kW = 100%)
	if (inverter_Power_Command != NULL)
	{
		double power_percent = *inverter_Power_Command; // Assume this is in percent (0-100)
		uint16_t power_output[2] = {0};
		invert_modbus_value(power_percent, DATA_TYPE_UINT16, SOLIS_SCALE_FACTOR_100, power_output);
		shm_reg_info->ptr[SOLIS_INV_OUTPUT_POWER_CONTROL - 1] = power_output[0];
		// log_message("Solis Inverter: Power command = %.2f %% (scaled value = %d)\n", power_percent, power_output[0]);
	}
}

void modify_solis_input_registers(const param_array_t *dynamic_data, MAYBE_UNUSED const param_array_t *fixed_data, shared_memory_info_t *shm_reg_info)
{
	// Static variables for input register status/readings
	static double *inverter_Active_Power = NULL;
	static double *inverter_Total_Energy = NULL;
	static double *inverter_Dc_Voltage_1 = NULL;
	static double *inverter_Dc_Current_1 = NULL;
	static double *inverter_Dc_Busbar_Voltage = NULL;
	static int *inverter_Status = NULL;

	static bool first_Run = false;
	if (!first_Run)
	{
		// Initialize pointers to simulation data
		get_param(dynamic_data, "inverter_active_power", &inverter_Active_Power);
		get_param(dynamic_data, "inverter_total_energy", &inverter_Total_Energy);
		get_param(dynamic_data, "inverter_dc_voltage_1", &inverter_Dc_Voltage_1);
		get_param(dynamic_data, "inverter_dc_current_1", &inverter_Dc_Current_1);
		get_param(dynamic_data, "inverter_dc_busbar_voltage", &inverter_Dc_Busbar_Voltage);
		get_param(dynamic_data, "inverter_status", &inverter_Status);
		first_Run = true;
	}

	// SOLIS_INV_ACTIVE_POWER_REALTIME (addresses 3005-3006) - Real-time active power (uint32)
	if (inverter_Active_Power != NULL)
	{
		uint16_t power_output[2] = {0};
		invert_modbus_value(*inverter_Active_Power, DATA_TYPE_UINT32, 1.0, power_output);
		shm_reg_info->ptr[SOLIS_INV_ACTIVE_POWER_REALTIME_LOW - 1] = power_output[0];
		shm_reg_info->ptr[SOLIS_INV_ACTIVE_POWER_REALTIME_HIGH - 1] = power_output[1];
		// log_message("Solis Inverter: Active power = %.2f W (registers = [%d, %d])\n", *inverter_Active_Power, power_output[0], power_output[1]);
	}

	// SOLIS_INV_TOTAL_ENERGY_OUTPUT (addresses 3009-3010) - Total energy in kWh (uint32)
	if (inverter_Total_Energy != NULL)
	{
		uint16_t energy_output[2] = {0};
		invert_modbus_value(*inverter_Total_Energy, DATA_TYPE_UINT32, 1.0, energy_output);
		shm_reg_info->ptr[SOLIS_INV_TOTAL_ENERGY_OUTPUT_LOW - 1] = energy_output[0];
		shm_reg_info->ptr[SOLIS_INV_TOTAL_ENERGY_OUTPUT_HIGH - 1] = energy_output[1];
		// log_message("Solis Inverter: Total energy = %.2f kWh (registers = [%d, %d])\n", *inverter_Total_Energy, energy_output[0], energy_output[1]);
	}

	// SOLIS_INV_DC_VOLTAGE_1 (address 3022) - DC Voltage 1 (scaled by 10)
	if (inverter_Dc_Voltage_1 != NULL)
	{
		uint16_t voltage_output[2] = {0};
		invert_modbus_value(*inverter_Dc_Voltage_1, DATA_TYPE_UINT16, SOLIS_SCALE_FACTOR_10, voltage_output);
		shm_reg_info->ptr[SOLIS_INV_DC_VOLTAGE_1 - 1] = voltage_output[0];
		// log_message("Solis Inverter: DC Voltage 1 = %.2f V (scaled value = %d)\n", *inverter_Dc_Voltage_1, voltage_output[0]);
	}

	// SOLIS_INV_DC_CURRENT_1 (address 3023) - DC Current 1 (scaled by 10)
	if (inverter_Dc_Current_1 != NULL)
	{
		uint16_t current_output[2] = {0};
		invert_modbus_value(*inverter_Dc_Current_1, DATA_TYPE_UINT16, SOLIS_SCALE_FACTOR_10, current_output);
		shm_reg_info->ptr[SOLIS_INV_DC_CURRENT_1 - 1] = current_output[0];
		// log_message("Solis Inverter: DC Current 1 = %.2f A (scaled value = %d)\n", *inverter_Dc_Current_1, current_output[0]);
	}

	// SOLIS_INV_DC_BUSBAR_VOLTAGE (address 3032) - DC Busbar Voltage (scaled by 10)
	if (inverter_Dc_Busbar_Voltage != NULL)
	{
		uint16_t busbar_voltage_output[2] = {0};
		invert_modbus_value(*inverter_Dc_Busbar_Voltage, DATA_TYPE_UINT16, SOLIS_SCALE_FACTOR_10, busbar_voltage_output);
		shm_reg_info->ptr[SOLIS_INV_DC_BUSBAR_VOLTAGE - 1] = busbar_voltage_output[0];
		// log_message("Solis Inverter: DC Busbar Voltage = %.2f V (scaled value = %d)\n", *inverter_Dc_Busbar_Voltage, busbar_voltage_output[0]);
	}

	// SOLIS_INV_STATUS (address 3044) - Inverter status code
	if (inverter_Status != NULL)
	{
		shm_reg_info->ptr[SOLIS_INV_STATUS - 1] = (uint16_t)(*inverter_Status);
		// log_message("Solis Inverter: Status = %d\n", *inverter_Status);
	}
}

void modify_solis_coils(MAYBE_UNUSED const param_array_t *dynamic_data, MAYBE_UNUSED const param_array_t *fixed_data, shared_memory_info_t *shm_reg_info)
{
	// Static variables for coil control
	static int *grid_Enable_Signal = NULL;

	static bool first_Run = false;
	if (!first_Run)
	{
		// Initialize pointer to simulation data
		get_param(dynamic_data, "grid_enable_signal", &grid_Enable_Signal);
		first_Run = true;
	}

	// SOLIS_GRID_ONOFF (coil 5001) - Grid connection enable/disable
	if (grid_Enable_Signal != NULL)
	{
		uint16_t grid_value = (*grid_Enable_Signal) ? 1 : 0;
		shm_reg_info->ptr[SOLIS_GRID_ONOFF - 1] = grid_value;
		// log_message("Solis Inverter: Grid enable = %d (%s)\n", grid_value, (*grid_Enable_Signal) ? "ENABLED" : "DISABLED");
	}
}

// ABB VFD-Specific Functions (Network 2, Server 1)

void modify_abb_vfd_holding_registers(const param_array_t *dynamic_data, const param_array_t *fixed_data, shared_memory_info_t *shm_reg_info)
{
	// Static variables for VFD data
	static double *vfd_Torque_Command = NULL;
	static double *omega = NULL;
	static double *gearbox_Ratio = NULL;
	static double *tau_Flow_Extract = NULL;
	static double *power_Extract = NULL;
	static double *motor_Gen_Rated_Torque_Nm = NULL;
	static double *time_Sec = NULL;
	static int *enable_Brake_Signal = NULL;

	static bool first_Run = false;
	if (!first_Run)
	{
		// Initialize pointers to simulation data
		get_param(dynamic_data, "vfd_torque_command", &vfd_Torque_Command);
		get_param(dynamic_data, "omega", &omega);
		get_param(dynamic_data, "tau_flow_extract", &tau_Flow_Extract);
		get_param(dynamic_data, "power_extract", &power_Extract);
		get_param(dynamic_data, "enable_brake_signal", &enable_Brake_Signal);
		get_param(dynamic_data, "time_sec", &time_Sec);
		get_param(fixed_data, "motor_gen_rated_torque_nm", &motor_Gen_Rated_Torque_Nm);
		get_param(fixed_data, "gearbox_ratio", &gearbox_Ratio);
		first_Run = true;
	}

	// Write feedback values TO shared memory (simulation -> VFD status registers)

	// ABB_VFD_SPEED_ESTIMATED (address 7) - Estimated speed in RPM (scaled by 10)
	if (omega != NULL && gearbox_Ratio != NULL)
	{
		const double speed_rpm_vfd = rads_to_rpm(*omega) * (*gearbox_Ratio) * -1.0;
		uint16_t speed_output[2] = {0};
		invert_modbus_value(speed_rpm_vfd, DATA_TYPE_INT16, ABB_VFD_SCALE_FACTOR_10, speed_output);
		shm_reg_info->ptr[ABB_VFD_SPEED_ESTIMATED - 1] = speed_output[0];
		// log_message("ABB VFD: Speed estimated = %.2f RPM (scaled value = %d)\n", speed_rpm_vfd, speed_output[0]);
	}

	// ABB_VFD_OUTPUT_POWER (address 10) - Output power in kW (scaled by 100)
	if (power_Extract != NULL && tau_Flow_Extract != NULL && omega != NULL)
	{
		*power_Extract = *tau_Flow_Extract * *omega;
		const double power_kw = (*power_Extract / 10) * -1.0; // Convert to kW
		uint16_t power_output[2] = {0};
		invert_modbus_value(power_kw, DATA_TYPE_INT16, ABB_VFD_SCALE_FACTOR_100, power_output);
		shm_reg_info->ptr[ABB_VFD_OUTPUT_POWER - 1] = power_output[0];
		// log_message("ABB VFD: Output power = %.2f kW (scaled value = %d)\n", power_kw, power_output[0]);
	}

	// Read command values FROM shared memory (VFD control registers -> simulation)

	// ABB_VFD_TORQUE_COMMAND (address 2) - Torque command in % (scaled by 100)
	if (vfd_Torque_Command != NULL && motor_Gen_Rated_Torque_Nm != NULL && tau_Flow_Extract != NULL && gearbox_Ratio != NULL)
	{
		uint16_t torque_command_received[2] = {0};
		torque_command_received[0] = shm_reg_info->ptr[ABB_VFD_TORQUE_COMMAND - 1];
		*vfd_Torque_Command = convert_modbus_value(torque_command_received, DATA_TYPE_INT16, ABB_VFD_SCALE_FACTOR_100);
		*tau_Flow_Extract = torque_percent_to_nm(*vfd_Torque_Command, *motor_Gen_Rated_Torque_Nm, *gearbox_Ratio);

		uint16_t vfd_control_word[2] = {0};
		vfd_control_word[0] = shm_reg_info->ptr[ABB_VFD_CONTROL_WORD - 1];

		static int first_Run_Vfd_State = 0;
		if (omega != NULL && time_Sec != NULL)
		{
			if (first_Run_Vfd_State == 0 && vfd_control_word[0] != 6)
			{
				*omega = 0;
				*tau_Flow_Extract = 0;
				*time_Sec = 0;
			}
			else if (first_Run_Vfd_State == 0 && vfd_control_word[0] == 6)
			{
				*omega = 0;
				*tau_Flow_Extract = 0;
				*time_Sec = 0;
				first_Run_Vfd_State = 1;
			}
		}

		// static uint16_t old_vfd_control_word[2] = {0};

		// if (old_vfd_control_word[0] != vfd_control_word[0])
		// {
		// 	log_message("time: %f, omega: %f, new control word %d --> %d\n", *time_Sec, *omega, old_vfd_control_word[0], vfd_control_word[0]);
		// 	old_vfd_control_word[0] = vfd_control_word[0];
		// }

		if (omega != NULL && time_Sec != NULL)
		{
			static double old_Torque_Command = 0;
			static double old_Rpm = 0;
			static double old_Time = 0;
			uint16_t speed_output[2] = {0};
			speed_output[0] = shm_reg_info->ptr[ABB_VFD_SPEED_ESTIMATED - 1];
			double rpm_vfd = convert_modbus_value(speed_output, DATA_TYPE_INT16, ABB_VFD_SCALE_FACTOR_10);
			if (*vfd_Torque_Command != old_Torque_Command || old_Rpm != rpm_vfd)
			{
				if (*vfd_Torque_Command != old_Torque_Command)
				{
					log_message("time: %f, time_diff: %f, *omega: %f, old_rpm: %f, rpm new: %f, RPM OR ABB VFD Torque command changed from %.2f%% to %.2f%%, *tau_flow_extract : %f\n", *time_Sec, *time_Sec - old_Time, *omega, old_Rpm, rpm_vfd, old_Torque_Command, *vfd_Torque_Command, *tau_Flow_Extract);
				}
				// else
				// {
				// 	log_message("time: %f, time_diff: %f, old_rpm: %f, rpm new: %f, RPM OR ABB VFD Torque command changed from %.2f%% to %.2f%%, *tau_flow_extract : %f      ---------------\n", *time_Sec, *time_Sec - old_Time, old_Rpm, rpm_vfd, old_Torque_Command, *vfd_Torque_Command, *tau_Flow_Extract);
				// }
				old_Torque_Command = *vfd_Torque_Command;
				old_Time = *time_Sec;
				old_Rpm = rpm_vfd;
			}
		}
	}
}

void modify_input_register_information(const param_array_t *dynamic_data, const param_array_t *fixed_data, shared_memory_info_t *shm_reg_info, const int server_id, const int network_id)
{
	// Dispatch to device-specific logic based on (network_id, server_id)
	if (network_id == 4 && server_id == 1)
	{
		// Network 4, Server 1: Solis Inverter
		modify_solis_input_registers(dynamic_data, fixed_data, shm_reg_info);
	}
	// Add other device logic here as needed
}

void shared_memory_controls_update(const param_array_t *dynamic_data, MAYBE_UNUSED const param_array_t *fixed_data)
{
	static double *vfd_Torque_Command = NULL;
	static double *omega = NULL;
	static int *enable_Brake_Signal = NULL;

	static bool first_Run = false;
	if (!first_Run)
	{
		// Initialize variables since this is the first time the function is running.
		get_param(dynamic_data, "vfd_torque_command", &vfd_Torque_Command);
		get_param(dynamic_data, "omega", &omega);
		get_param(dynamic_data, "enable_brake_signal", &enable_Brake_Signal);

		first_Run = true;
	}

	// Update simulation sensor data for GPIO/PRU backends
#ifndef _WIN32
	update_simulation_sensor_data(dynamic_data);
#endif

	check_status_of_child(childPID);
	// log_message("We are in shared_memory_controls_update, nextDeviceSlot: %d\n", nextDeviceSlot);
	for (int bus_num = 0; bus_num < nextDeviceSlot; bus_num++)
	{
		for (modbus_type_index_t mb_idx = COILS; mb_idx < NUMBER_MODBUS_TYPES; mb_idx++)
		{
			shared_memory_sem_info_t *shm_sem_info = modbusDevices[bus_num].shm_sem_info;
			if (shm_sem_info[mb_idx].mapping_count_values > 0)
			{
				// Acquire the semaphore before accessing shared registers
				if (shmem_wait_check(&shm_sem_info[mb_idx].sem_info, shm_sem_info[mb_idx].modbus_type))
				{
					return;
				}

				// if shm_sem_info[mb_idx].shm_shared_size-1 is greater than 0 then there is a new write from the user.
				if (shm_sem_info[mb_idx].shm_info.ptr[shm_sem_info[mb_idx].shm_shared_size - 2] > 0)
				{
					// log_message("shm_sem_info[mb_idx].shm_info.ptr[shm_sem_info[mb_idx].shm_shared_size - 2](%d) > 0, shm_sem_info[mb_idx].num_values: %d\n", shm_sem_info[mb_idx].shm_info.ptr[shm_sem_info[mb_idx].shm_shared_size - 2], shm_sem_info[mb_idx].num_values);
					for (int j = 0; j < shm_sem_info[mb_idx].num_values; j++)
					{
						int k = shm_sem_info[mb_idx].values[j][0];
						shm_sem_info[mb_idx].values[j][1] = shm_sem_info[mb_idx].shm_info.ptr[k - 1];
						// log_message("shm_sem_info[mb_idx].shm_info.ptr[%d]: %d\n", k - 1, shm_sem_info[mb_idx].shm_info.ptr[k - 1]);
					}

					shm_sem_info[mb_idx].shm_info.ptr[shm_sem_info[mb_idx].shm_shared_size - 2] = 0;
					shm_sem_info[mb_idx].shm_info.ptr[shm_sem_info[mb_idx].shm_shared_size - 1] = 0;
				}

				switch (mb_idx)
				{
				case COILS:
					modify_coil_information(dynamic_data, fixed_data, &shm_sem_info[mb_idx].shm_info, modbusDevices[bus_num].server_id, modbusDevices[bus_num].network_id);
					break;
				case INPUT_COILS:
					modify_input_coil_information(dynamic_data, fixed_data, &shm_sem_info[mb_idx].shm_info, modbusDevices[bus_num].server_id, modbusDevices[bus_num].network_id);
					break;
				case HOLDING_REGISTERS:
					modify_holding_register_information(dynamic_data, fixed_data, &shm_sem_info[mb_idx].shm_info, modbusDevices[bus_num].server_id, modbusDevices[bus_num].network_id);
					break;
				case INPUT_REGISTERS:
					modify_input_register_information(dynamic_data, fixed_data, &shm_sem_info[mb_idx].shm_info, modbusDevices[bus_num].server_id, modbusDevices[bus_num].network_id);
					break;
				default:
					ERROR_MESSAGE("Invalid index!\n");
					exit(EXIT_FAILURE);
				}

				// Release the semaphore after accessing shared values
				if (shmem_post_check(&shm_sem_info[mb_idx].sem_info, shm_sem_info[mb_idx].modbus_type))
				{
					return;
				}
			}
		}
	}
}

// ============================================================================
// GPIO/PRU Simulation Support
// ============================================================================

#ifndef _WIN32
// Shared memory info for simulation sensor data
static shared_memory_info_t simSensorShmemInfo = {0};
static sim_sensor_data_t *simSensorData = NULL;

// Semaphore for simulation sensor data
static semaphore_info_t simSensorSemInfo = {0};

/**
 * @brief	Create and initialize the simulation sensor data shared memory segment
 *
 * This shared memory is used to pass simulated sensor values from the simulation
 * (this file) to the simulation backends (gpio_input_output_sim, pru_frequency_input_sim).
 */
void create_simulation_sensor_data_shmem(void)
{
	// Setup shared memory info structure
	safe_snprintf(simSensorShmemInfo.name, sizeof(simSensorShmemInfo.name), "%s", SIM_SENSOR_DATA_SHM_NAME);
	simSensorShmemInfo.size = sizeof(sim_sensor_data_t);

	// Create shared memory using xflow utilities
	create_shared_memory_sim(&simSensorShmemInfo, SHMEM_CREATE);

	// Cast the pointer to our structure type
	simSensorData = (sim_sensor_data_t *)simSensorShmemInfo.ptr;

	// Setup semaphore info structure
	safe_snprintf(simSensorSemInfo.name, sizeof(simSensorSemInfo.name), "%s_sem", SIM_SENSOR_DATA_SHM_NAME);

	// Create semaphore using xflow utilities
	create_semaphore(&simSensorSemInfo, SEMAPHORE_CREATE);

	// Initialize the data structure (with semaphore protection)
	shmem_wait_check(&simSensorSemInfo, SIM_SENSOR_TYPE);

	// Initialize all values to 0/OFF
	safe_memset(simSensorData->gpio_pins, 0, sizeof(simSensorData->gpio_pins));
	safe_memset(simSensorData->coil_pins, 0, sizeof(simSensorData->coil_pins));
	safe_memset(simSensorData->pru_freq_hz, 0, sizeof(simSensorData->pru_freq_hz));
	simSensorData->last_update_us = 0;

	shmem_post_check(&simSensorSemInfo, SIM_SENSOR_TYPE);

	log_message("Created simulation sensor data shared memory: %s\n", SIM_SENSOR_DATA_SHM_NAME);
}

/**
 * @brief	Update simulation sensor data from simulation variables
 *
 * This function is called every simulation timestep to update the shared memory
 * with current simulated sensor values.
 */
void update_simulation_sensor_data(const param_array_t *dynamic_data)
{
	if (simSensorData == NULL)
	{
		return;
	}

	static double *omega = NULL;
	static int *enable_Brake_Signal = NULL;

	static bool first_Run = false;
	if (!first_Run)
	{
		// Get pointers to simulation variables
		get_param(dynamic_data, "omega", &omega);
		get_param(dynamic_data, "enable_brake_signal", &enable_Brake_Signal);
		first_Run = true;
	}

	// Acquire semaphore
	shmem_wait_check(&simSensorSemInfo, SIM_SENSOR_TYPE);

	// Update PRU frequency from omega
	// omega is in rad/s, convert to Hz: freq = omega / (2*PI)
	if (omega != NULL)
	{
		simSensorData->pru_freq_hz[0] = (*omega) / M_PI;
	}

	if (simSensorData->coil_pins[12] == GPIO_OFF)
	{
		simSensorData->gpio_pins[27] = GPIO_OFF;
		*enable_Brake_Signal = 1;
	}
	else
	{
		simSensorData->gpio_pins[27] = GPIO_ON;
		*enable_Brake_Signal = 0;
	}

	simSensorData->gpio_pins[28] = simSensorData->coil_pins[14];

	// Update timestamp
	simSensorData->last_update_us = (uint64_t)(timespec_to_double(get_monotonic_timestamp()) * USEC_PER_SEC);

	// Release semaphore
	shmem_post_check(&simSensorSemInfo, SIM_SENSOR_TYPE);
}

/**
 * @brief	Cleanup simulation shared memory
 */
void cleanup_simulation_sensor_data(void)
{
	if (simSensorData != NULL)
	{
		// Destroy shared memory using xflow utilities
		destroy_shared_memory_xflow(&simSensorShmemInfo);

		// Destroy semaphore using xflow utilities
		destroy_semaphore(&simSensorSemInfo);

		simSensorData = NULL;
		log_message("Cleaned up simulation sensor data shared memory\n");
	}
}

#endif // _WIN32
