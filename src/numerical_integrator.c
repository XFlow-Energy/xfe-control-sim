/**
 * @file    numerical_integrator.c
 * @author  XFlow Energy
 * @date    2025
 * @brief   Numerical Integrator functions
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

#include "numerical_integrator.h"
#include "equation_of_motion.h" // for eom
#include "logger.h"             // for ERROR_MESSAGE
#include "make_stage.h"         // for MAKE_STAGE_DEFINE
#include "xflow_core.h"         // for shutdownFlag
#include <stdbool.h>            // IWYU pragma: keep
#include <stddef.h>             // for NULL
#include <stdlib.h>             // for free, malloc

// expand definitions once, using both the decl‐list and the call‐list
MAKE_STAGE_DEFINE(numerical_integrator, void, (NUMERICAL_INTEGRATOR_PARAM_LIST), (NUMERICAL_INTEGRATOR_CALL_ARGS))

/**
 * @brief Advances a system of ODE state variables using a 2nd-order Adams–Bashforth integrator with a Heun starter.
 *
 * On the first invocation, uses Heun’s method (a 2nd-order Runge–Kutta) to compute the first step:
 *   1. Evaluates f(x₀) → k1
 *   2. Computes x* = x₀ + dt·k1
 *   3. Evaluates f(x*) → k2
 *   4. Sets x₁ = x₀ + (dt/2)·(k1 + k2) and stores prev_Dx = k2
 *
 * On subsequent invocations, performs the AB2 step:
 *   xₙ₊₁ = xₙ + (dt/2)·[3·f(xₙ) – f(xₙ₋₁)],
 *   updating prev_Dx for the next step.
 *
 * @param state_vars     Array of pointers to the current state variables xₙ (length n_state_var).
 *                       Each element is updated in place to xₙ₊₁.
 * @param state_names    Array of parameter names corresponding to each state variable (for diagnostics).
 * @param n_state_var    Number of state variables.
 * @param dt             Time step size.
 * @param dynamic_data   Pointer to dynamic parameters used by the ODE right‐hand side (`eom`).
 * @param fixed_data     Pointer to fixed parameters used by the ODE right‐hand side (`eom`).
 *
 * @note
 * - Allocates a temporary buffer `dx` of length `n_state_var` to hold f(xₙ).
 * - On the first call, allocates additional buffers for Heun’s method and frees them before returning.
 * - On error (malloc failure), logs via `ERROR_MESSAGE()`, sets `shutdownFlag = 1`, and returns.
 */
void ab2_numerical_integrator(NUMERICAL_INTEGRATOR_PARAM_LIST)
{
	static double *prev_Dx = NULL; // f(x_{n-1})
	static bool first_Call = true;

	double *dx = malloc(n_state_var * sizeof(double));
	if (!dx)
	{
		ERROR_MESSAGE("AB2 integrator: failed to allocate dx buffer.\n");
		shutdownFlag = 1;
		return;
	}

	if (first_Call)
	{
		// --- Heun starter (2nd-order) ---
		double *k1 = malloc(n_state_var * sizeof(double));
		double *k2 = malloc(n_state_var * sizeof(double));
		double *x_temp_values = malloc(n_state_var * sizeof(double));
		double **x_ptrs = (double **)malloc(n_state_var * sizeof(double *));
		if (!k1 || !k2 || !x_temp_values || !x_ptrs)
		{
			ERROR_MESSAGE("AB2 integrator: failed to allocate Heun buffers.\n");
			shutdownFlag = 1;
			free(k1);
			free(k2);
			free(x_temp_values);
			free((void *)x_ptrs);
			free(dx);
			return;
		}

		for (int i = 0; i < n_state_var; ++i)
		{
			x_ptrs[i] = &x_temp_values[i];
		}

		// 1) k1 = f(x₀)
		eom(state_vars, state_names, n_state_var, k1, dynamic_data, fixed_data);

		// 2) x* = x₀ + dt * k1
		for (int i = 0; i < n_state_var; ++i)
		{
			x_temp_values[i] = *state_vars[i] + (dt * k1[i]);
		}

		// 3) k2 = f(x*)
		eom(x_ptrs, state_names, n_state_var, k2, dynamic_data, fixed_data);

		// allocate prev_Dx
		prev_Dx = malloc(n_state_var * sizeof(double));
		if (!prev_Dx)
		{
			ERROR_MESSAGE("AB2 integrator: failed to allocate prev_Dx.\n");
			shutdownFlag = 1;
			free(k1);
			free(k2);
			free(x_temp_values);
			free((void *)x_ptrs);
			free(dx);
			return;
		}

		// 4) x₁ = x₀ + dt/2 * (k1 + k2), seed prev_Dx = k2
		for (int i = 0; i < n_state_var; ++i)
		{
			*state_vars[i] += dt * 0.5 * (k1[i] + k2[i]);
			prev_Dx[i] = k2[i];
		}

		first_Call = false;
		free(k1);
		free(k2);
		free(x_temp_values);
		free((void *)x_ptrs);
		free(dx);
		return;
	}

	// Normal AB2 step
	eom(state_vars, state_names, n_state_var, dx, dynamic_data, fixed_data);

	// x_{n+1} = x_n + dt/2 * (3f(x_n) - f(x_{n-1}))
	for (int i = 0; i < n_state_var; ++i)
	{
		*state_vars[i] += dt * 0.5 * (3.0 * dx[i] - prev_Dx[i]);
	}

	// Update prev_Dx for next step
	for (int i = 0; i < n_state_var; ++i)
	{
		prev_Dx[i] = dx[i];
	}

	free(dx);
}

/**
 * @brief Advances a system of ODE state variables using the forward Euler method.
 *
 * Allocates a temporary buffer `dx` of length `n_state_var`, computes the time derivatives
 * at the current state by calling `eom()`, then updates each state variable in place:
 * \f[
 *   x_i \;\gets\; x_i + dt \cdot \dot{x}_i
 * \f]
 * Finally, frees the temporary buffer.
 *
 * @param state_vars     Array of pointers to the current state variables (length \c n_state_var);
 *                       each pointer is updated to the new value.
 * @param state_names    Array of \c n_state_var null-terminated strings naming each state variable
 *                       (used by \c eom for diagnostics).
 * @param n_state_var    Number of state variables.
 * @param dt             Time step size.
 * @param dynamic_data   Pointer to dynamic parameters passed through to the ODE right-hand side (\c eom).
 * @param fixed_data     Pointer to fixed parameters passed through to the ODE right-hand side (\c eom).
 *
 * @note
 * - On memory allocation failure for the \c dx buffer, logs an error via \c ERROR_MESSAGE(),
 *   sets \c shutdownFlag = 1, and returns without updating the state.
 */
void euler_numerical_integrator(NUMERICAL_INTEGRATOR_PARAM_LIST)
{
	double *dx = malloc(n_state_var * sizeof(double));
	if (!dx)
	{
		ERROR_MESSAGE("Euler integrator: failed to allocate dx.\n");
		shutdownFlag = 1;
		return;
	}

	// Evaluate derivatives at the current state
	eom(state_vars, state_names, n_state_var, dx, dynamic_data, fixed_data);

	// Apply forward Euler update: x = x + dt * dx
	for (int i = 0; i < n_state_var; ++i)
	{
		*state_vars[i] += dt * dx[i];
	}

	free(dx);
}

/**
 * @brief Advances ODE state variables using the classical 4th-order Runge–Kutta method.
 *
 * Allocates temporary buffers `k1`–`k4` and `temp` (all length `n_state_var`), then computes:
 *   1. `k1 = f(xₙ)`
 *   2. `k2 = f(xₙ + (dt/2)·k1)`
 *   3. `k3 = f(xₙ + (dt/2)·k2)`
 *   4. `k4 = f(xₙ + dt·k3)`
 * Finally updates each state variable in place:
 * \f[
 *   x_{n+1} = x_n + \frac{dt}{6}\bigl(k1 + 2k2 + 2k3 + k4\bigr).
 * \f]
 * Frees all temporary buffers on exit.
 *
 * @param state_vars     Array of pointers to the current state variables xₙ (length \c n_state_var);
 *                       each pointer is updated to xₙ₊₁.
 * @param state_names    Array of \c n_state_var null-terminated names for each state variable.
 * @param n_state_var    Number of state variables.
 * @param dt             Time step size.
 * @param dynamic_data   Pointer to dynamic parameters passed through to the ODE right-hand side (\c eom).
 * @param fixed_data     Pointer to fixed parameters passed through to the ODE right-hand side (\c eom).
 *
 * @note
 * - If any `malloc()` for the temporary buffers fails, logs an error via `ERROR_MESSAGE()`,
 *   sets `shutdownFlag = 1`, and skips the update (but still frees any allocated buffers).
 */
void rk4_numerical_integrator(NUMERICAL_INTEGRATOR_PARAM_LIST)
{
	double *k1 = malloc(n_state_var * sizeof(double));
	double *k2 = malloc(n_state_var * sizeof(double));
	double *k3 = malloc(n_state_var * sizeof(double));
	double *k4 = malloc(n_state_var * sizeof(double));
	double *temp = malloc(n_state_var * sizeof(double));

	if (!k1 || !k2 || !k3 || !k4 || !temp)
	{
		ERROR_MESSAGE("RK4 integrator allocation failed.\n");
		shutdownFlag = 1;
		goto cleanup;
	}

	// Save original state x_n
	for (int i = 0; i < n_state_var; ++i)
	{
		temp[i] = *state_vars[i];
	}

	// k1 = f(x_n)
	eom(state_vars, state_names, n_state_var, k1, dynamic_data, fixed_data);

	// state = x_n + (dt/2) * k1
	for (int i = 0; i < n_state_var; ++i)
	{
		*state_vars[i] = temp[i] + (0.5 * dt * k1[i]);
	}
	eom(state_vars, state_names, n_state_var, k2, dynamic_data, fixed_data);

	// state = x_n + (dt/2) * k2
	for (int i = 0; i < n_state_var; ++i)
	{
		*state_vars[i] = temp[i] + (0.5 * dt * k2[i]);
	}
	eom(state_vars, state_names, n_state_var, k3, dynamic_data, fixed_data);

	// state = x_n + dt * k3
	for (int i = 0; i < n_state_var; ++i)
	{
		*state_vars[i] = temp[i] + (dt * k3[i]);
	}
	eom(state_vars, state_names, n_state_var, k4, dynamic_data, fixed_data);

	// Final update: x_{n+1} = x_n + (dt/6)*(k1 + 2*k2 + 2*k3 + k4)
	for (int i = 0; i < n_state_var; ++i)
	{
		*state_vars[i] = temp[i] + ((dt / 6.0) * (k1[i] + (2 * k2[i]) + (2 * k3[i]) + k4[i]));
	}

cleanup:
	free(k1);
	free(k2);
	free(k3);
	free(k4);
	free(temp);
}
