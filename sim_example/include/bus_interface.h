/**
 * @file    bus_interface.h
 * @author  XFlow Energy
 * @date    2025
 * @brief   API for bus_interface
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

#ifndef BUS_INTERFACE_H
#define BUS_INTERFACE_H

#include "xfe_control_sim_version.h"
#include "xfe_control_sim_common.h"
#include "xflow_core.h"
#include "xflow_aero_sim.h"
#include <sys/types.h>

#ifdef _WIN32
#include <windows.h>
// Only under MSVC do we need to supply pid_t
#ifdef _MSC_VER
typedef DWORD pid_t;
#endif
#endif

// Solis Inverter Modbus Register Addresses
typedef enum
{
	SOLIS_INV_ONOFF = 3007,                // Holding register: Inverter On/Off command (1=on, 190=off)
	SOLIS_INV_OUTPUT_POWER_CONTROL = 3052, // Holding register: Output power control (scaled by 100, in %kW)
} solis_holding_register_address_t;

typedef enum
{
	SOLIS_INV_ACTIVE_POWER_REALTIME_LOW = 3005,  // Input register: Real-time active power (uint32 low word)
	SOLIS_INV_ACTIVE_POWER_REALTIME_HIGH = 3006, // Input register: Real-time active power (uint32 high word)
	SOLIS_INV_TOTAL_ENERGY_OUTPUT_LOW = 3009,    // Input register: Total energy output kWh (uint32 low word)
	SOLIS_INV_TOTAL_ENERGY_OUTPUT_HIGH = 3010,   // Input register: Total energy output kWh (uint32 high word)
	SOLIS_INV_DC_VOLTAGE_1 = 3022,               // Input register: DC Voltage 1 (scaled by 10, in DCV)
	SOLIS_INV_DC_CURRENT_1 = 3023,               // Input register: DC Current 1 (scaled by 10, in A)
	SOLIS_INV_DC_VOLTAGE_2 = 3024,               // Input register: DC Voltage 2 (scaled by 10, in DCV)
	SOLIS_INV_DC_CURRENT_2 = 3025,               // Input register: DC Current 2 (scaled by 10, in A)
	SOLIS_INV_DC_VOLTAGE_3 = 3026,               // Input register: DC Voltage 3 (scaled by 10, in DCV)
	SOLIS_INV_DC_CURRENT_3 = 3027,               // Input register: DC Current 3 (scaled by 10, in A)
	SOLIS_INV_DC_VOLTAGE_4 = 3028,               // Input register: DC Voltage 4 (scaled by 10, in DCV)
	SOLIS_INV_DC_CURRENT_4 = 3029,               // Input register: DC Current 4 (scaled by 10, in A)
	SOLIS_INV_DC_BUSBAR_VOLTAGE = 3032,          // Input register: DC Busbar Voltage (scaled by 10, in DCV)
	SOLIS_INV_DC_BUSBAR_HALF_VOLTAGE = 3033,     // Input register: DC Busbar Half Voltage (scaled by 10, in DCV)
	SOLIS_INV_STATUS = 3044,                     // Input register: Inverter status code
} solis_input_register_address_t;

typedef enum
{
	SOLIS_GRID_ONOFF = 5001, // Coil: Grid connection enable/disable
} solis_coil_address_t;

typedef enum
{
	SOLIS_SCALE_FACTOR_10 = 10,   // Scale factor for voltage/current (divide by 10)
	SOLIS_SCALE_FACTOR_100 = 100, // Scale factor for power control (divide by 100)
} solis_scale_factor_t;

// ABB VFD Modbus Register Addresses
typedef enum
{
	// Status/Feedback registers (input=TRUE - read from VFD)
	ABB_VFD_SPEED_ESTIMATED = 7,         // Holding register: Estimated speed (scaled by 10, in RPM)
	ABB_VFD_OUTPUT_TORQUE = 8,           // Holding register: Output torque (scaled by 100, in %)
	ABB_VFD_OUTPUT_AC_VOLTAGE = 9,       // Holding register: Output AC voltage (scaled by 1, in VAC)
	ABB_VFD_OUTPUT_POWER = 10,           // Holding register: Output power (scaled by 100, in kW)
	ABB_VFD_STATUS_WORD_2 = 11,          // Holding register: Status word 2
	ABB_VFD_DC_VOLTAGE = 12,             // Holding register: DC bus voltage (scaled by 10, in VDC)
	ABB_VFD_SPEED_HZ = 13,               // Holding register: Speed in Hz (scaled by 10, in HZ)
	ABB_VFD_OUTPUT_CURRENT = 14,         // Holding register: Output current (scaled by 100, in A)
	ABB_VFD_RECENT_TRIPPING_FAULT = 401, // Holding register: Recent tripping fault code

	// Command/Control registers (input=FALSE - write to VFD)
	ABB_VFD_CONTROL_WORD = 1,              // Holding register: Control word
	ABB_VFD_TORQUE_COMMAND = 2,            // Holding register: Torque command (scaled by 100, in %)
	ABB_VFD_SPEED_COMMAND = 3,             // Holding register: Speed command (scaled by 10, in RPM)
	ABB_VFD_FREQUENCY_COMMAND = 3,         // Holding register: Frequency command (scaled by 100, in RPM) - same address as speed
	ABB_VFD_TORQUE_OR_FREQ_CONTROL = 1911, // Holding register: Torque or frequency control mode
	ABB_VFD_LOCAL_CONTROL_ENABLE = 1917,   // Holding register: Local control enable/disable
	ABB_VFD_EXTERNAL_ERROR_ENABLE = 3101,  // Holding register: External error enable/disable
	ABB_VFD_SPEED_SCALING = 4601,          // Holding register: Speed scaling factor
} abb_vfd_holding_register_address_t;

typedef enum
{
	ABB_VFD_SCALE_FACTOR_1 = 1,     // No scaling
	ABB_VFD_SCALE_FACTOR_10 = 10,   // Scale factor for speed/voltage (divide by 10)
	ABB_VFD_SCALE_FACTOR_100 = 100, // Scale factor for torque/power/current (divide by 100)
} abb_vfd_scale_factor_t;

void shared_memory_controls_update(const param_array_t *dynamic_data, const param_array_t *fixed_data);
#ifdef _WIN32
pid_t launch_modbus_server_windows(char *modbus_server_executable_location_program_name, char *network_id_str, char *device_csv_file, char *server_ip, char *tcp_port, char *timeout_us);
#else
pid_t launch_modbus_server_unix(char *modbus_server_executable_location_program_name, char *network_id_str, char *device_csv_file, char *server_ip, char *tcp_port, char *timeout_us);
#endif

void launch_shared_mem_and_hardware_interface(void);

// Main device dispatch functions
void modify_holding_register_information(const param_array_t *dynamic_data, const param_array_t *fixed_data, shared_memory_info_t *shm_reg_info, const int server_id, const int network_id);
void modify_coil_information(const param_array_t *dynamic_data, const param_array_t *fixed_data, shared_memory_info_t *shm_reg_info, const int server_id, const int network_id);
void modify_input_coil_information(const param_array_t *dynamic_data, const param_array_t *fixed_data, shared_memory_info_t *shm_reg_info, const int server_id, const int network_id);
void modify_input_register_information(const param_array_t *dynamic_data, const param_array_t *fixed_data, shared_memory_info_t *shm_reg_info, const int server_id, const int network_id);

// Solis inverter-specific functions (Network 4, Server 1)
void modify_solis_holding_registers(const param_array_t *dynamic_data, const param_array_t *fixed_data, shared_memory_info_t *shm_reg_info);
void modify_solis_input_registers(const param_array_t *dynamic_data, const param_array_t *fixed_data, shared_memory_info_t *shm_reg_info);
void modify_solis_coils(const param_array_t *dynamic_data, const param_array_t *fixed_data, shared_memory_info_t *shm_reg_info);

// ABB VFD-specific functions (Network 2, Server 1)
void modify_abb_vfd_holding_registers(const param_array_t *dynamic_data, const param_array_t *fixed_data, shared_memory_info_t *shm_reg_info);

// GPIO/PRU simulation support functions
#ifndef _WIN32
void create_simulation_sensor_data_shmem(void);
void update_simulation_sensor_data(const param_array_t *dynamic_data);
void cleanup_simulation_sensor_data(void);
#endif

#endif
