/**
 * @file    numerical_integrator.h
 * @author  XFlow Energy
 * @date    2025
 * @brief   API for numerical_integrator
 */

/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * XFLOW-CONTROL-SIM
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

#ifndef NUMERICAL_INTEGRATOR_H
#define NUMERICAL_INTEGRATOR_H

#include "make_stage.h"     // for MAKE_STAGE
#include "maybe_unused.h"   // for MAYBE_UNUSED
#include "xflow_aero_sim.h" // for param_array_t

// your one and only definition of the parameter list:
#define NUMERICAL_INTEGRATOR_PARAM_LIST MAYBE_UNUSED double **state_vars, MAYBE_UNUSED const char **state_names, MAYBE_UNUSED const int n_state_var, MAYBE_UNUSED const double dt, MAYBE_UNUSED const param_array_t *dynamic_data, MAYBE_UNUSED const param_array_t *fixed_data
#define NUMERICAL_INTEGRATOR_CALL_ARGS state_vars, state_names, n_state_var, dt, dynamic_data, fixed_data

// now invoke MAKE_STAGE with the *macro* name inside parens
MAKE_STAGE(numerical_integrator, void, (NUMERICAL_INTEGRATOR_PARAM_LIST))

void ab2_numerical_integrator(NUMERICAL_INTEGRATOR_PARAM_LIST);
void euler_numerical_integrator(NUMERICAL_INTEGRATOR_PARAM_LIST);
void rk4_numerical_integrator(NUMERICAL_INTEGRATOR_PARAM_LIST);

static const numerical_integrator_Map numericalIntegratorMap[] = {
	{"ab2_numerical_integrator",   ab2_numerical_integrator  },
	{"euler_numerical_integrator", euler_numerical_integrator},
	{"rk4_numerical_integrator",   rk4_numerical_integrator  },
};

#endif // NUMERICAL_INTEGRATOR_H
