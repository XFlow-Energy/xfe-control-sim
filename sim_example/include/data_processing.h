/**
 * @file    data_processing.h
 * @author  XFlow Energy
 * @date    2025
 * @brief   API for data_processing
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

#ifndef DATA_PROCESSING_H
#define DATA_PROCESSING_H

#include "xflow_control_sim_version.h"
#include "xflow_control_sim_common.h"
#include "maybe_unused.h"
#include "xflow_core.h"
#include "xflow_aero_sim.h"

#ifdef _WIN32
#define SEM_CLOSE(sem) CloseHandle(sem)
#else
#define SEM_CLOSE(sem) sem_close(sem)
#endif

#ifdef _WIN32
#include "xflow_core.h"

// Only under MSVC do we need to supply pid_t
#if defined(_MSC_VER)
typedef DWORD pid_t;
#endif
#endif

#define SEM_NAME_FMT_DP "/dps_%s"
#include "make_stage.h"

typedef enum
{
	BEGINNING,
	LOOPING,
	ENDING
} data_processing_operation_t;

// your one and only definition of the parameter list:
#define DATA_PROCESSING_PARAM_LIST MAYBE_UNUSED const param_array_t *dynamic_data, MAYBE_UNUSED const param_array_t *fixed_data, MAYBE_UNUSED data_processing_program_args_t *dp_program_options
#define DATA_PROCESSING_CALL_ARGS dynamic_data, fixed_data, dp_program_options

// now invoke MAKE_STAGE with the *macro* name inside parens
MAKE_STAGE(data_processing, void, (DATA_PROCESSING_PARAM_LIST))

void example_data_processing(DATA_PROCESSING_PARAM_LIST);

static const data_processing_Map dataProcessingMap[] = {
	{"example_data_processing", example_data_processing},
};

#endif
