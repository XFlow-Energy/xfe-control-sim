/**
 * @file    equation_of_motion.h
 * @author  XFlow Energy
 * @date    2025
 * @brief   API for eom
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

#ifndef EQUATION_OF_MOTION_H
#define EQUATION_OF_MOTION_H

#include "xfe_control_sim_version.h"
#include "xfe_control_sim_common.h"
#include "xflow_core.h"
#include "xflow_aero_sim.h"
#include "make_stage.h"

// your one and only definition of the parameter list:
#define EOM_PARAM_LIST MAYBE_UNUSED double **state_vars, MAYBE_UNUSED const char **state_names, MAYBE_UNUSED const int n_state_var, MAYBE_UNUSED double *dx, MAYBE_UNUSED const param_array_t *dynamic_data, MAYBE_UNUSED const param_array_t *fixed_data
#define EOM_CALL_ARGS state_vars, state_names, n_state_var, dx, dynamic_data, fixed_data

// now invoke MAKE_STAGE with the *macro* name inside parens
MAKE_STAGE(eom, void, (EOM_PARAM_LIST))

void eom_simple_ball_thrown_in_air(EOM_PARAM_LIST);
void example_turbine_eom(EOM_PARAM_LIST);

static const eom_Map eomMap[] = {
	{"eom_simple_ball_thrown_in_air", eom_simple_ball_thrown_in_air},
	{"example_turbine_eom",           example_turbine_eom          },
};

#endif
