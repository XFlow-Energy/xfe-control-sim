/**
 * @file    actuator.c
 * @brief   Passthrough actuator stub
 *
 * No-op implementation. Custom projects override with real actuator models.
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

#include "actuators.h"

MAKE_STAGE_DEFINE(actuator, void, (ACTUATOR_PARAM_LIST), (ACTUATOR_CALL_ARGS))

void passthrough_actuator(ACTUATOR_PARAM_LIST)
{
	// No-op: commands pass through directly
}
