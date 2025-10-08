/**
 * @file    xfe_control_sim_common.h
 * @author  XFlow Energy
 * @date    2025
 * @brief   API for common simulation functions
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
#ifndef XFE_CONTROL_SIM_COMMON_H
#define XFE_CONTROL_SIM_COMMON_H

#include "maybe_unused.h"    // for MAYBE_UNUSED
#include "xflow_aero_sim.h"  // for param_array_t, bts_data_t, input_param_...
#include "xflow_shmem_sem.h" // for semaphore_info_t
#include <stdbool.h>         // IWYU pragma: keep

#ifdef _WIN32
#ifdef XFE_CONTROL_SIM_LIB_EXPORTS
#define XFE_CONTROL_SIM_API __declspec(dllexport)
#else
#define XFE_CONTROL_SIM_API __declspec(dllimport)
#endif
#else
#define XFE_CONTROL_SIM_API
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Define the shared memory name based on the platform.
#ifdef _WIN32
MAYBE_UNUSED static const char *shmemName = "Local\\precomputed_wind_interp";
#else
MAYBE_UNUSED static const char *shmemName = "/precomputed_wind_interp";
#endif

enum child_status
{
	CHILD_STILL_RUNNING = -1,
	CHILD_ERROR_STATUS = -2
};

typedef struct
{
	int argc;
	const char **argv;
} data_processing_program_args_t;

typedef enum
{
	CSV_LOGGER_INIT,
	CSV_LOGGER_LOG,
	CSV_LOGGER_CLOSE
} csv_logger_action_t;

void save_velocity_to_csv(bts_data_t *data, const double horizontal_y_position, const double vertical_z_position, const char *file_path, const char *base_filename);
void print_velocity_for_yz(bts_data_t *data, int iy, int iz);
void print_velocity_for_y_z_position(bts_data_t *data, const double horizontal_y_position, double vertical_z_position);
void save_umag_velocity_data_to_csv(const double *vel_data, int num_time_steps, const char *file_path, const char *base_filename, double dt);
double get_closest_umag(const double *vel_data, int num_time_steps, double current_time, double dt);
void save_param_array_data_to_csv(const char *filename, const param_array_t *data, int write_header);
void dynamic_data_csv_logger(FILE **file, const csv_logger_action_t action, const char *filename, const param_array_t *data);

int get_param_value(const param_array_t *data, const char *name, input_param_type_t *type, void *value);
void initialize_data(param_array_t *dynamic_data, param_array_t *fixed_data);
void save_dynamic_fixed_data_at_shutdown(const param_array_t *dynamic_data, const param_array_t *fixed_data, const bool logging_status);
void initialize_control_system(param_array_t **dynamic_data, param_array_t **fixed_data, history_task_list_t **out_task_list, const bool logging_status);
void continuous_logging_function(const param_array_t *dynamic_data, const param_array_t *fixed_data);
void load_double_struct_param(const param_array_t *data, const char *param_name, double *param);
void create_shared_interp(const double *precomputed_wind_interp, int num_sim_steps);
void destroy_shared_interp(void);
double *get_shared_interp(const char *name, int num_sim_steps);

void add_data_to_array(double *array, const long sim_points_count, const int index, const int final_dp_index, const double *value_ptr);
int get_num_cores(void);
int compare_doubles(const void *a, const void *b);
int check_parent_status(const int parent_pid);
int check_duplicate_status_of_child(const int child_pid);
double update_cpu_usage(void);
void save_csv_header(const char *filename, semaphore_info_t *sem_info, const char **headers);
void save_double_array_data_to_csv(const char *filename, semaphore_info_t *sem_info, const double *data, int n_data);

#endif
