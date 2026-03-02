/**
 * @file    sensor.c
 * @brief   Passthrough sensor stub
 *
 * No-op implementations. Custom projects override with real sensor models.
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

#include "sensors.h"

MAKE_STAGE_DEFINE(sensor, void, (SENSOR_PARAM_LIST), (SENSOR_CALL_ARGS))

void passthrough_sensor(SENSOR_PARAM_LIST)
{
	// No-op: sensor values equal truth
}

void sensor_inject(const param_array_t *dynamic_data, const param_array_t *fixed_data)
{
	(void)dynamic_data;
	(void)fixed_data;
	// No-op: nothing to inject when using passthrough sensor
}
