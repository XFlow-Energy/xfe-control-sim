/**
 * @file    control_switch.c
 * @author  XFlow Energy
 * @date    2025
 * @brief   Switch between different functions specified in the csv file
 * Used for testing the sim software and different control algorithm, kw^2
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
#include "control_switch.h"
#include "data_processing.h"      // for dataProcessingMap, register_data...
#include "drivetrains.h"          // for drivetrainMap, register_drivetrain
#include "equation_of_motion.h"   // for eomMap, register_eom
#include "flow_gen.h"             // for flowMap, register_flow_gen
#include "flow_sim_model.h"       // for flowSimModelMap, register_flow_...
#include "make_stage.h"           // for DEFINE_STAGE_DISPATCHER, DISPATCH_...
#include "numerical_integrator.h" // for numericalIntegratorMap, register...
#include "turbine_controls.h"     // for turbineControlMap, register_turb...
#include <stdbool.h>              // IWYU pragma: keep
#include <stddef.h>               // for NULL

DEFINE_STAGE_DISPATCHER(flow_gen, flowMap)
DEFINE_STAGE_DISPATCHER(numerical_integrator, numericalIntegratorMap)
DEFINE_STAGE_DISPATCHER(turbine_control, turbineControlMap)
DEFINE_STAGE_DISPATCHER(eom, eomMap)
DEFINE_STAGE_DISPATCHER(drivetrain, drivetrainMap)
DEFINE_STAGE_DISPATCHER(flow_sim_model, flowSimModelMap)
DEFINE_STAGE_DISPATCHER(data_processing, dataProcessingMap)

void control_switch(CONTROL_SWITCH_PARAM_LIST)
{
	static const char *flow_Function_Call = NULL;
	static const char *numerical_Integrator_Function_Call = NULL;
	static const char *turbine_Control_Function_Call = NULL;
	static const char *eom_Function_Call = NULL;
	static const char *drivetrain_Function_Call = NULL;
	static const char *flow_Sim_Model_Function_Call = NULL;
	static const char *data_Processing_Function_Call = NULL;

	static bool first_Run = false;
	if (!first_Run)
	{
		get_param(fixed_data, "flow_function_call", &flow_Function_Call);
		get_param(fixed_data, "numerical_integrator_function_call", &numerical_Integrator_Function_Call);
		get_param(fixed_data, "turbine_control_function_call", &turbine_Control_Function_Call);
		get_param(fixed_data, "eom_function_call", &eom_Function_Call);
		get_param(fixed_data, "drivetrain_function_call", &drivetrain_Function_Call);
		get_param(fixed_data, "flow_sim_model_function_call", &flow_Sim_Model_Function_Call);
		get_param(fixed_data, "data_processing_function_call", &data_Processing_Function_Call);

		// log_message("flow_Function_Call: %s\n", flow_Function_Call);

		// this runs the generic loop & sets the callback, or errors:
		DISPATCH_STAGE_OR_ERROR(flow_gen, flowMap, flow_Function_Call);
		DISPATCH_STAGE_OR_ERROR(numerical_integrator, numericalIntegratorMap, numerical_Integrator_Function_Call);
		DISPATCH_STAGE_OR_ERROR(turbine_control, turbineControlMap, turbine_Control_Function_Call);
		DISPATCH_STAGE_OR_ERROR(eom, eomMap, eom_Function_Call);
		DISPATCH_STAGE_OR_ERROR(drivetrain, drivetrainMap, drivetrain_Function_Call);
		DISPATCH_STAGE_OR_ERROR(flow_sim_model, flowSimModelMap, flow_Sim_Model_Function_Call);
		DISPATCH_STAGE_OR_ERROR(data_processing, dataProcessingMap, data_Processing_Function_Call);

		first_Run = true;
	}
}
