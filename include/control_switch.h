/**
 * @file    turbine_controls.h
 * @author  XFlow Energy
 * @date    2025
 * @brief   API for turbine_control
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
#ifndef CONTROL_SWITCH_H
#define CONTROL_SWITCH_H

#include "maybe_unused.h"   // for MAYBE_UNUSED
#include "xflow_aero_sim.h" // for param_array_t

#define CONTROL_SWITCH_PARAM_LIST MAYBE_UNUSED const param_array_t *dynamic_data, MAYBE_UNUSED const param_array_t *fixed_data

void control_switch(CONTROL_SWITCH_PARAM_LIST);

#endif
