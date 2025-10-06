/**
 * @file    turbine_controls.h
 * @author  XFlow Energy
 * @date    2025
 * @brief   API for turbine_control
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

#ifndef TURBINE_CONTROLS_H
#define TURBINE_CONTROLS_H

// NOLINTBEGIN(llvm-include-order)
#include "xflow_core.h"
#include "xfe_control_sim_version.h"
#include "xfe_control_sim_common.h"
#include "xflow_aero_sim.h"
#include "make_stage.h"
// NOLINTEND(llvm-include-order)

// your one and only definition of the parameter list:
#define TURBINE_CONTROL_PARAM_LIST MAYBE_UNUSED const param_array_t *dynamic_data, MAYBE_UNUSED const param_array_t *fixed_data
#define TURBINE_CONTROL_CALL_ARGS dynamic_data, fixed_data

// now invoke MAKE_STAGE with the *macro* name inside parens
MAKE_STAGE(turbine_control, void, (TURBINE_CONTROL_PARAM_LIST))

void example_turbine_control(TURBINE_CONTROL_PARAM_LIST);
void kw2_turbine_control(TURBINE_CONTROL_PARAM_LIST);

static const turbine_control_Map turbineControlMap[] = {
	{"example_turbine_control", example_turbine_control},
	{"kw2_turbine_control",     kw2_turbine_control    },
};

#endif
