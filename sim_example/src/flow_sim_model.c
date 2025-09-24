/**
 * @file    flow_sim_model.c
 * @author  XFlow Energy
 * @date    2025
 * @brief   Aero model
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

#include "xflow_aero_sim.h" // for get_param, param_array_t
#include "flow_sim_model.h" // for flow_sim_model
#include "logger.h"         // IWYU pragma: keep
#include "make_stage.h"
#include <math.h>    // for pow, fabs
#include <stdbool.h> // IWYU pragma: keep
#include <stddef.h>  // for NULL

// expand definitions once, using both the decl‐list and the call‐list
MAKE_STAGE_DEFINE(flow_sim_model, void, (FLOW_SIM_MODEL_PARAM_LIST), (FLOW_SIM_MODEL_CALL_ARGS))

// Structure to hold turbine data
typedef struct
{
	double radius;  // Turbine radius
	double area;    // Turbine frontal area
	double slow_cq; // cq for very low speeds or backwards
	double rho;     // Air density
} turbine_data_t;

// Function to calculate aerodynamic torque
static double tau_flow_calc(double omega, double u, const turbine_data_t *turb_dat)
{
	if (u <= 0)
	{
		return 0; // No wind, no torque
	}

	if (omega <= 0)
	{
		// No/low rotation torque (use slow_cq for very low speeds)
		return turb_dat->slow_cq * 0.5 * turb_dat->rho * pow(u, 2) * turb_dat->area * turb_dat->radius;
	}

	double tsr = omega * turb_dat->radius / u; // Tip speed ratio
	if (tsr < 0)
	{
		tsr = 0;
	}

	double cp = (-0.1 * (tsr - 3) * (tsr - 3)) + 0.5; // oversimplified cp equation

	double cq = cp / tsr; // Torque coefficient

	// Low speed/no speed correction
	if (fabs(cq) < turb_dat->slow_cq)
	{
		cq = turb_dat->slow_cq;
	}
	// Aero torque
	return cq * 0.5 * turb_dat->rho * pow(u, 2) * turb_dat->area * turb_dat->radius;
}

void example_flow_sim_model(FLOW_SIM_MODEL_PARAM_LIST)
{
	static double *omega = NULL;
	static double *flow_Speed = NULL;
	static double *tau_Flow = NULL;

	static double *radius = NULL;
	static double *area = NULL;
	static double *slow_Cq = NULL;
	static double *rho = NULL;

	static turbine_data_t turb_Dat;

	static bool first_Run = false;
	if (!first_Run)
	{
		// initialize variables since this is the first time the function is running.
		get_param(dynamic_data, "omega", &omega);
		get_param(dynamic_data, "flow_speed", &flow_Speed);
		get_param(dynamic_data, "tau_flow", &tau_Flow);

		get_param(fixed_data, "R", &radius);
		get_param(fixed_data, "A", &area);
		get_param(fixed_data, "slowCQ", &slow_Cq);
		get_param(fixed_data, "rho", &rho);

		// Set global turbine data (turb_Dat)
		turb_Dat.radius = *radius;
		turb_Dat.area = *area;
		turb_Dat.slow_cq = *slow_Cq;
		turb_Dat.rho = *rho;

		first_Run = true;
	}

	// Get aerodynamic torque
	*tau_Flow = tau_flow_calc(*omega, *flow_Speed, &turb_Dat);

	// Log results
	// log_message("Calculated aerodynamic torque  omega: %f, u: %f, tau_flow: %f\n", *omega, *flow_Speed, *tau_Flow);
}
