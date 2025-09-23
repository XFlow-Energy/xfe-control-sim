/**
 * @file    flow_gen.h
 * @author  XFlow Energy
 * @date    2025
 * @brief   API for flow_gen
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
#ifndef FLOW_GEN_H
#define FLOW_GEN_H

#include "make_stage.h"     // for MAKE_STAGE
#include "maybe_unused.h"   // for MAYBE_UNUSED
#include "xflow_aero_sim.h" // for param_array_t

// your one and only definition of the parameter list:
#define FLOW_GEN_PARAM_LIST MAYBE_UNUSED const param_array_t *dynamic_data, MAYBE_UNUSED const param_array_t *fixed_data
#define FLOW_GEN_CALL_ARGS dynamic_data, fixed_data

MAKE_STAGE(flow_gen, void, (FLOW_GEN_PARAM_LIST))

void bts_fixed_interp_flow_gen(FLOW_GEN_PARAM_LIST);
void csv_fixed_interp_flow_gen(FLOW_GEN_PARAM_LIST);

static const flow_gen_Map flowMap[] = {
	{"csv_fixed_interp_flow_gen", csv_fixed_interp_flow_gen},
	{"bts_fixed_interp_flow_gen", bts_fixed_interp_flow_gen},
};

#endif // FLOW_GEN_H
