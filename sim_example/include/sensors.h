/**
 * @file    sensors.h
 * @author  XFlow Energy
 * @date    2025
 * @brief   API for sensor stage (passthrough stub)
 *
 * Provides a passthrough sensor that leaves all variables at their true
 * values. Custom projects override this header with real sensor models.
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

#ifndef SENSORS_H
#define SENSORS_H

// NOLINTBEGIN(llvm-include-order)
#include "xflow_core.h"
#include "xfe_control_sim_version.h"
#include "xfe_control_sim_common.h"
#include "xflow_aero_sim.h"
#include "make_stage.h"
// NOLINTEND(llvm-include-order)

#define SENSOR_PARAM_LIST MAYBE_UNUSED const param_array_t *dynamic_data, MAYBE_UNUSED const param_array_t *fixed_data
#define SENSOR_CALL_ARGS dynamic_data, fixed_data

MAKE_STAGE(sensor, void, (SENSOR_PARAM_LIST))

void passthrough_sensor(SENSOR_PARAM_LIST);

// Inject: overwrite live variables with sensed companions before the controller.
// Passthrough stub is a no-op.
void sensor_inject(const param_array_t *dynamic_data, const param_array_t *fixed_data);

static const sensor_Map sensorMap[] = {
	{"passthrough_sensor", passthrough_sensor},
};

#endif
