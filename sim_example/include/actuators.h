/**
 * @file    actuators.h
 * @author  XFlow Energy
 * @date    2025
 * @brief   API for actuator stage (passthrough stub)
 *
 * Provides a passthrough actuator that applies commands directly.
 * Custom projects override this header with real actuator models.
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

#ifndef ACTUATORS_H
#define ACTUATORS_H

// NOLINTBEGIN(llvm-include-order)
#include "xflow_core.h"
#include "xfe_control_sim_version.h"
#include "xfe_control_sim_common.h"
#include "xflow_aero_sim.h"
#include "make_stage.h"
// NOLINTEND(llvm-include-order)

#define ACTUATOR_PARAM_LIST MAYBE_UNUSED const param_array_t *dynamic_data, MAYBE_UNUSED const param_array_t *fixed_data
#define ACTUATOR_CALL_ARGS dynamic_data, fixed_data

MAKE_STAGE(actuator, void, (ACTUATOR_PARAM_LIST))

void passthrough_actuator(ACTUATOR_PARAM_LIST);

static const actuator_Map actuatorMap[] = {
	{"passthrough_actuator", passthrough_actuator},
};

#endif
