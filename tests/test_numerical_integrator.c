/**
 * @file    test_numerical_integrator.c
 * @brief   Test suite for numerical integrator functions
 * @author  XFlow Energy
 * @date    2025
 */

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "equation_of_motion.h"
#include "numerical_integrator.h"
#include "xfe_control_sim_common.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// dx/dt = -x : simple linear decay with analytic solution x(t) = x0 * e^(-t).
// Gives every integrator finite, well-behaved output and deterministic assertions.
static void test_decay_eom(EOM_PARAM_LIST)
{
	for (int i = 0; i < n_state_var; ++i)
	{
		dx[i] = -(*state_vars[i]);
	}
}

// Test tracking globals
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;
static int verbose_mode = 0;

// Color codes
#define COLOR_RED "\x1b[31m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_BLUE "\x1b[34m"
#define COLOR_CYAN "\x1b[36m"
#define COLOR_RESET "\x1b[0m"

#define EPSILON 1e-10

#define VERBOSE_PRINT(...)       \
	do                           \
	{                            \
		if (verbose_mode)        \
		{                        \
			printf(__VA_ARGS__); \
		}                        \
	} while (0)

#define TEST_ASSERT(condition, msg)                            \
	do                                                         \
	{                                                          \
		if (!(condition))                                      \
		{                                                      \
			printf(COLOR_RED "  FAIL: %s\n" COLOR_RESET, msg); \
			tests_failed++;                                    \
			return 0;                                          \
		}                                                      \
	} while (0)

#define RUN_TEST(test_func)                           \
	do                                                \
	{                                                 \
		tests_run++;                                  \
		printf("Running: %s ... ", #test_func);       \
		if (test_func())                              \
		{                                             \
			printf(COLOR_GREEN "PASS\n" COLOR_RESET); \
			tests_passed++;                           \
		}                                             \
	} while (0)

#define TEST_PASS 1

// ====================================================================================
// Test Cases: Basic Integrator Function Existence and Signatures
// ====================================================================================

// One-step sanity tests against the analytic decay solution x(1*dt) = e^(-dt).
// Bounds are intentionally loose because they're just verifying the integrator
// stepped in the right direction with the right rough magnitude — the dedicated
// accuracy tests at the bottom of this file pin down the precise error orders.

int test_euler_integrator_exists()
{
	double x = 1.0;
	double *state_vars[] = {&x};
	const char *state_names[] = {"x"};
	const double dt = 0.01;
	// Euler one step: x_new = x + dt * (-x) = 0.99 exactly.
	const double expected = 0.99;

	euler_numerical_integrator(state_vars, state_names, 1, dt, NULL, NULL);

	TEST_ASSERT(!isnan(x) && !isinf(x), "Euler integrator should not produce NaN/Inf");
	TEST_ASSERT(fabs(x - expected) < 1e-12, "Euler one step matches closed form");
	VERBOSE_PRINT("  Euler one step: x=%.12f (expected %.12f)\n", x, expected);
	return TEST_PASS;
}

int test_rk4_integrator_exists()
{
	double x = 1.0;
	double *state_vars[] = {&x};
	const char *state_names[] = {"x"};
	const double dt = 0.01;
	// RK4 one step on dx/dt=-x is essentially exact at this size.
	const double expected = exp(-dt);

	rk4_numerical_integrator(state_vars, state_names, 1, dt, NULL, NULL);

	TEST_ASSERT(!isnan(x) && !isinf(x), "RK4 integrator should not produce NaN/Inf");
	TEST_ASSERT(fabs(x - expected) < 1e-9, "RK4 one step matches analytic e^(-dt)");
	VERBOSE_PRINT("  RK4 one step: x=%.12f (analytic %.12f)\n", x, expected);
	return TEST_PASS;
}

int test_ab2_integrator_exists()
{
	double x = 1.0;
	double *state_vars[] = {&x};
	const char *state_names[] = {"x"};
	const double dt = 0.01;
	// First AB2 call uses Heun's method (2nd-order):
	//   k1 = -x = -1
	//   x* = 1 + 0.01*(-1) = 0.99
	//   k2 = -0.99
	//   x_new = 1 + 0.005*(-1 + -0.99) = 1 - 0.00995 = 0.99005
	const double expected = 0.99005;

	ab2_numerical_integrator(state_vars, state_names, 1, dt, NULL, NULL);

	TEST_ASSERT(!isnan(x) && !isinf(x), "AB2 integrator should not produce NaN/Inf");
	TEST_ASSERT(fabs(x - expected) < 1e-12, "AB2 first step matches Heun closed form");
	VERBOSE_PRINT("  AB2 first step: x=%.12f (expected %.12f)\n", x, expected);
	return TEST_PASS;
}

// ====================================================================================
// Test Cases: Edge Cases
// ====================================================================================

int test_integrators_handle_zero_timestep()
{
	// dt=0 multiplies dx by 0, so state must be bitwise-unchanged.
	double x = 5.0;
	double *state_vars[] = {&x};
	const char *state_names[] = {"x"};
	const double x_initial = x;

	euler_numerical_integrator(state_vars, state_names, 1, 0.0, NULL, NULL);

	TEST_ASSERT(x == x_initial, "Zero timestep must leave state exactly unchanged");
	VERBOSE_PRINT("  Zero timestep: x=%.16f (was %.16f)\n", x, x_initial);
	return TEST_PASS;
}

int test_integrators_handle_multiple_state_vars()
{
	// dx/dt = -x  =>  after dt, x_i → x_i * e^(-dt). RK4 nails this at dt=0.01.
	double x[5] = {1.0, 2.0, 3.0, 4.0, 5.0};
	const double x_initial[5] = {1.0, 2.0, 3.0, 4.0, 5.0};
	double *state_vars[] = {&x[0], &x[1], &x[2], &x[3], &x[4]};
	const char *state_names[] = {"x0", "x1", "x2", "x3", "x4"};
	const double dt = 0.01;
	const double decay = exp(-dt);

	rk4_numerical_integrator(state_vars, state_names, 5, dt, NULL, NULL);

	for (int i = 0; i < 5; i++)
	{
		double expected = x_initial[i] * decay;
		TEST_ASSERT(!isnan(x[i]) && !isinf(x[i]), "x[i] must be finite");
		TEST_ASSERT(fabs(x[i] - expected) < 1e-9, "x[i] tracks analytic decay");
		VERBOSE_PRINT("  x[%d]=%.12f (expected %.12f)\n", i, x[i], expected);
	}
	return TEST_PASS;
}

int test_integrators_handle_single_state_var()
{
	double x = 10.0;
	double *state_vars[] = {&x};
	const char *state_names[] = {"position"};
	const double dt = 0.1;
	const double expected = 10.0 * exp(-dt);

	rk4_numerical_integrator(state_vars, state_names, 1, dt, NULL, NULL);

	TEST_ASSERT(!isnan(x) && !isinf(x), "Single state variable should remain finite");
	TEST_ASSERT(fabs(x - expected) < 1e-6, "Single state variable tracks analytic decay");
	VERBOSE_PRINT("  Single var: x=%.12f (expected %.12f)\n", x, expected);
	return TEST_PASS;
}

int test_integrators_handle_negative_timestep()
{
	// Negative dt integrates backwards in time. With dx/dt=-x, going backwards
	// makes x grow toward x*e^(+|dt|).
	double x = 1.0;
	double *state_vars[] = {&x};
	const char *state_names[] = {"x"};
	const double dt = -0.01;
	const double expected = exp(-dt); // exp(+0.01) ≈ 1.01005

	rk4_numerical_integrator(state_vars, state_names, 1, dt, NULL, NULL);

	TEST_ASSERT(!isnan(x) && !isinf(x), "Negative timestep should not produce NaN/Inf");
	TEST_ASSERT(x > 1.0, "Negative timestep should grow x (backwards decay)");
	TEST_ASSERT(fabs(x - expected) < 1e-9, "Backwards step matches analytic exp(+|dt|)");
	VERBOSE_PRINT("  Negative dt: x=%.12f (expected %.12f)\n", x, expected);
	return TEST_PASS;
}

// ====================================================================================
// Test Cases: Stability Tests
// ====================================================================================

int test_integrators_repeated_calls()
{
	// Test that integrators can be called multiple times in sequence
	double x = 1.0;
	double *state_vars[] = {&x};
	const char *state_names[] = {"x"};
	int n_state_var = 1;
	double dt = 0.001;

	// Call integrator 100 times
	for (int i = 0; i < 100; i++)
	{
		rk4_numerical_integrator(state_vars, state_names, n_state_var, dt, NULL, NULL);

		// Check stability on each step
		if (isnan(x) || isinf(x))
		{
			TEST_ASSERT(0, "Integrator should remain stable over multiple calls");
		}
	}

	TEST_ASSERT(!isnan(x) && !isinf(x), "Integrator should be stable after many steps");

	VERBOSE_PRINT("  Repeated calls handled correctly\n");

	return TEST_PASS;
}

int test_euler_vs_rk4_different_results()
{
	// Euler and RK4 should generally produce different results (different order methods)
	// This tests that they're actually different implementations

	double x_euler = 1.0;
	double x_rk4 = 1.0;
	double *state_vars_euler[] = {&x_euler};
	double *state_vars_rk4[] = {&x_rk4};
	const char *state_names[] = {"x"};
	int n_state_var = 1;
	double dt = 0.1; // Larger timestep to see difference

	// Take one step with each integrator
	euler_numerical_integrator(state_vars_euler, state_names, n_state_var, dt, NULL, NULL);
	rk4_numerical_integrator(state_vars_rk4, state_names, n_state_var, dt, NULL, NULL);

	// The results should generally be different (unless eom is trivial like dx/dt = 0)
	// We can't assert this strongly without knowing the EOM, but it's informative
	VERBOSE_PRINT("  Euler result: %.10f, RK4 result: %.10f\n", x_euler, x_rk4);

	// At minimum, both should be finite
	TEST_ASSERT(!isnan(x_euler) && !isinf(x_euler), "Euler result should be finite");
	TEST_ASSERT(!isnan(x_rk4) && !isinf(x_rk4), "RK4 result should be finite");

	return TEST_PASS;
}

// ====================================================================================
// Test Cases: Map Validation
// ====================================================================================

int test_numerical_integrator_map_has_entries()
{
	int num_entries = sizeof(numericalIntegratorMap) / sizeof(numericalIntegratorMap[0]);

	TEST_ASSERT(num_entries >= 3, "numericalIntegratorMap should have at least 3 entries (RK4, Euler, AB2)");

	VERBOSE_PRINT("  numericalIntegratorMap has %d entries\n", num_entries);

	return TEST_PASS;
}

int test_integrator_map_contains_expected_functions()
{
	// Verify specific known integrators are in the map
	int num_entries = sizeof(numericalIntegratorMap) / sizeof(numericalIntegratorMap[0]);

	int found_rk4 = 0;
	int found_euler = 0;
	int found_ab2 = 0;

	for (int i = 0; i < num_entries; i++)
	{
		if (strcmp(numericalIntegratorMap[i].id, "rk4_numerical_integrator") == 0)
		{
			found_rk4 = 1;
			TEST_ASSERT(numericalIntegratorMap[i].fn == rk4_numerical_integrator, "RK4 function pointer should match");
		}
		else if (strcmp(numericalIntegratorMap[i].id, "euler_numerical_integrator") == 0)
		{
			found_euler = 1;
			TEST_ASSERT(numericalIntegratorMap[i].fn == euler_numerical_integrator, "Euler function pointer should match");
		}
		else if (strcmp(numericalIntegratorMap[i].id, "ab2_numerical_integrator") == 0)
		{
			found_ab2 = 1;
			TEST_ASSERT(numericalIntegratorMap[i].fn == ab2_numerical_integrator, "AB2 function pointer should match");
		}
	}

	TEST_ASSERT(found_rk4, "RK4 integrator should be in map");
	TEST_ASSERT(found_euler, "Euler integrator should be in map");
	TEST_ASSERT(found_ab2, "AB2 integrator should be in map");

	VERBOSE_PRINT("  Found all expected integrators in map\n");

	return TEST_PASS;
}

// ====================================================================================
// Test Cases: Numerical Accuracy vs Analytic Solution
// ====================================================================================
//
// With test_decay_eom (dx/dt = -x), the analytic solution is x(t) = x0 * e^(-t).
// We integrate from t=0 to t=1 with x0=1 and compare each integrator's final value
// to the true value e^(-1) ≈ 0.36788.
//
// AB2 is intentionally excluded from accuracy testing: it's a multistep method
// that holds f(x_{n-1}) in function-static state across the entire process. That
// is correct behavior for a continuous sim, but it means a unit test cannot get a
// clean reading on AB2 accuracy if any other AB2 call has happened first.

typedef void (*integrator_fn)(NUMERICAL_INTEGRATOR_PARAM_LIST);

static double integrate_decay(integrator_fn integrator, double x0, double dt, int n_steps)
{
	double x = x0;
	double *state_vars[] = {&x};
	const char *state_names[] = {"x"};
	for (int i = 0; i < n_steps; ++i)
	{
		integrator(state_vars, state_names, 1, dt, NULL, NULL);
	}
	return x;
}

int test_euler_accuracy_decay()
{
	const double dt = 0.001;
	const int n_steps = 1000; // t_final = 1.0
	const double analytic = exp(-1.0);

	double x_final = integrate_decay(euler_numerical_integrator, 1.0, dt, n_steps);
	double error = fabs(x_final - analytic);

	VERBOSE_PRINT("  Euler:  x(1)=%.10f  analytic=%.10f  err=%.3e\n", x_final, analytic, error);

	// Euler is O(h). At h=0.001, error scales like ~h/2 = 5e-4 for this ODE.
	TEST_ASSERT(error < 1e-3, "Euler error within O(h) bound");
	return TEST_PASS;
}

int test_rk4_accuracy_decay()
{
	const double dt = 0.001;
	const int n_steps = 1000;
	const double analytic = exp(-1.0);

	double x_final = integrate_decay(rk4_numerical_integrator, 1.0, dt, n_steps);
	double error = fabs(x_final - analytic);

	VERBOSE_PRINT("  RK4:    x(1)=%.10f  analytic=%.10f  err=%.3e\n", x_final, analytic, error);

	// RK4 is O(h^4). At h=0.001, error should be ~1e-13 — essentially machine precision.
	TEST_ASSERT(error < 1e-10, "RK4 error within O(h^4) bound");
	return TEST_PASS;
}

int test_ab2_accuracy_decay()
{
	const double dt = 0.001;
	const int n_steps = 1000;
	const double analytic = exp(-1.0);

	double x_final = integrate_decay(ab2_numerical_integrator, 1.0, dt, n_steps);
	double error = fabs(x_final - analytic);

	VERBOSE_PRINT("  AB2:    x(1)=%.10f  analytic=%.10f  err=%.3e\n", x_final, analytic, error);

	// AB2 is O(h^2). At h=0.001, error scales like ~h^2 = 1e-6.
	TEST_ASSERT(error < 1e-5, "AB2 error within O(h^2) bound");
	return TEST_PASS;
}

int test_integrator_accuracy_ordering()
{
	const double dt = 0.001;
	const int n_steps = 1000;
	const double analytic = exp(-1.0);

	double x_euler = integrate_decay(euler_numerical_integrator, 1.0, dt, n_steps);
	double x_ab2 = integrate_decay(ab2_numerical_integrator, 1.0, dt, n_steps);
	double x_rk4 = integrate_decay(rk4_numerical_integrator, 1.0, dt, n_steps);

	double err_euler = fabs(x_euler - analytic);
	double err_ab2 = fabs(x_ab2 - analytic);
	double err_rk4 = fabs(x_rk4 - analytic);

	VERBOSE_PRINT("  err_euler=%.3e  err_ab2=%.3e  err_rk4=%.3e\n", err_euler, err_ab2, err_rk4);

	TEST_ASSERT(err_rk4 < err_ab2, "RK4 should be more accurate than AB2");
	TEST_ASSERT(err_ab2 < err_euler, "AB2 should be more accurate than Euler");
	return TEST_PASS;
}

int test_euler_convergence_order()
{
	// Halving dt should roughly halve the error for a 1st-order method.
	const double analytic = exp(-1.0);

	double x_h = integrate_decay(euler_numerical_integrator, 1.0, 0.01, 100);
	double x_h2 = integrate_decay(euler_numerical_integrator, 1.0, 0.005, 200);

	double err_h = fabs(x_h - analytic);
	double err_h2 = fabs(x_h2 - analytic);
	double ratio = err_h / err_h2;

	VERBOSE_PRINT("  err(h=0.01)=%.3e  err(h=0.005)=%.3e  ratio=%.2f (expect ~2 for O(h))\n", err_h, err_h2, ratio);

	// Ratio for a 1st-order method should be close to 2. Allow a generous window
	// (1.5–3.0) since this is a finite-step empirical estimate, not asymptotic.
	TEST_ASSERT(ratio > 1.5 && ratio < 3.0, "Euler convergence order ≈ 1");
	return TEST_PASS;
}

// ====================================================================================
// Main Test Runner
// ====================================================================================

int main(int argc, char *argv[])
{
	if (argc > 1 && strcmp(argv[1], "-v") == 0)
	{
		verbose_mode = 1;
	}

	register_eom(test_decay_eom);

#ifdef RUN_ONLY_TEST
	// Per-test isolated build: only RUN_ONLY_TEST runs in this process.
	// CMake builds one binary per test function with -DRUN_ONLY_TEST=<name>
	// so that stateful integrators (AB2 holds f(x_{n-1}) in function-static
	// storage) start fresh each binary invocation.
#define _STR(x) #x
#define _STR2(x) _STR(x)
	tests_run = 1;
	if (RUN_ONLY_TEST())
	{
		tests_passed = 1;
		printf(COLOR_GREEN "PASS: %s\n" COLOR_RESET, _STR2(RUN_ONLY_TEST));
		return 0;
	}
	else
	{
		tests_failed = 1;
		printf(COLOR_RED "FAIL: %s\n" COLOR_RESET, _STR2(RUN_ONLY_TEST));
		return 1;
	}
#endif

	printf(COLOR_CYAN "\n");
	printf("=================================================================\n");
	printf("  Numerical Integrator Test Suite\n");
	printf("=================================================================\n");
	printf(COLOR_RESET "\n");

	// Basic function existence tests
	printf(COLOR_YELLOW "--- Function Existence Tests ---\n" COLOR_RESET);
	RUN_TEST(test_euler_integrator_exists);
	RUN_TEST(test_rk4_integrator_exists);
	RUN_TEST(test_ab2_integrator_exists);

	// Map validation tests
	printf(COLOR_YELLOW "\n--- Map Validation Tests ---\n" COLOR_RESET);
	RUN_TEST(test_numerical_integrator_map_has_entries);
	RUN_TEST(test_integrator_map_contains_expected_functions);

	// Edge case tests
	printf(COLOR_YELLOW "\n--- Edge Case Tests ---\n" COLOR_RESET);
	RUN_TEST(test_integrators_handle_zero_timestep);
	RUN_TEST(test_integrators_handle_multiple_state_vars);
	RUN_TEST(test_integrators_handle_single_state_var);
	RUN_TEST(test_integrators_handle_negative_timestep);

	// Stability tests
	printf(COLOR_YELLOW "\n--- Stability Tests ---\n" COLOR_RESET);
	RUN_TEST(test_integrators_repeated_calls);
	RUN_TEST(test_euler_vs_rk4_different_results);

	// Accuracy tests vs analytic solution (dx/dt = -x  =>  x(t) = e^(-t))
	printf(COLOR_YELLOW "\n--- Numerical Accuracy Tests ---\n" COLOR_RESET);
	RUN_TEST(test_euler_accuracy_decay);
	RUN_TEST(test_rk4_accuracy_decay);
	RUN_TEST(test_ab2_accuracy_decay);
	RUN_TEST(test_integrator_accuracy_ordering);
	RUN_TEST(test_euler_convergence_order);

	// Print summary
	printf(COLOR_CYAN "\n");
	printf("=================================================================\n");
	printf("  Test Summary\n");
	printf("=================================================================\n");
	printf(COLOR_RESET);
	printf("  Tests run:    %d\n", tests_run);
	printf("  Tests passed: " COLOR_GREEN "%d" COLOR_RESET "\n", tests_passed);
	printf("  Tests failed: " COLOR_RED "%d" COLOR_RESET "\n", tests_failed);
	printf("\n");

	if (tests_failed == 0)
	{
		printf(COLOR_GREEN "All tests passed!\n" COLOR_RESET);
		return 0;
	}
	else
	{
		printf(COLOR_RED "Some tests failed.\n" COLOR_RESET);
		return 1;
	}
}
