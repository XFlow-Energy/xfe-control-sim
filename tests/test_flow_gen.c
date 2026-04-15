/**
 * @file    test_flow_gen.c
 * @brief   Test suite for flow generation helper functions
 * @author  XFlow Energy
 * @date    2025
 */

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "xfe_control_sim_common.h"
#include "xflow_aero_sim.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// Tolerances
#define EPSILON 1e-6
#define PERCENT_TOLERANCE 0.01 // 1%

// Verbose output
#define VERBOSE_PRINT(...)       \
	do                           \
	{                            \
		if (verbose_mode)        \
		{                        \
			printf(__VA_ARGS__); \
		}                        \
	} while (0)

// Test assertions
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

#define TEST_ASSERT_DOUBLE_EQ(actual, expected, tolerance, msg)                                                                                        \
	do                                                                                                                                                 \
	{                                                                                                                                                  \
		if (fabs((actual) - (expected)) > (tolerance))                                                                                                 \
		{                                                                                                                                              \
			printf(COLOR_RED "  FAIL: %s (expected %.10f, got %.10f, error %.10e)\n" COLOR_RESET, msg, expected, actual, fabs((actual) - (expected))); \
			tests_failed++;                                                                                                                            \
			return 0;                                                                                                                                  \
		}                                                                                                                                              \
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
// Test Cases: interpolate_umag function
// ====================================================================================

int test_interpolate_umag_exact_match()
{
	// Test interpolation at exact data points
	double vel_data[] = {1.0, 2.0, 3.0, 4.0, 5.0};
	int num_time_steps = 5;
	double dt = 0.1;

	// Query at exact time points
	for (int i = 0; i < num_time_steps; i++)
	{
		double current_time = i * dt;
		double result = interpolate_umag(vel_data, num_time_steps, current_time, dt);

		VERBOSE_PRINT("  t=%.2f: expected=%.2f, got=%.2f\n", current_time, vel_data[i], result);
		TEST_ASSERT_DOUBLE_EQ(result, vel_data[i], EPSILON, "Exact time point interpolation");
	}

	return TEST_PASS;
}

int test_interpolate_umag_linear_interpolation()
{
	// Test linear interpolation between points
	double vel_data[] = {0.0, 10.0, 20.0, 30.0};
	int num_time_steps = 4;
	double dt = 1.0;

	// Query at midpoint between 10.0 and 20.0 (should be 15.0)
	double current_time = 1.5;
	double result = interpolate_umag(vel_data, num_time_steps, current_time, dt);
	double expected = 15.0;

	VERBOSE_PRINT("  Midpoint: t=%.2f, expected=%.2f, got=%.2f\n", current_time, expected, result);
	TEST_ASSERT_DOUBLE_EQ(result, expected, EPSILON, "Linear interpolation midpoint");

	// Query at 1/4 point between 0.0 and 10.0 (should be 2.5)
	current_time = 0.25;
	result = interpolate_umag(vel_data, num_time_steps, current_time, dt);
	expected = 2.5;

	VERBOSE_PRINT("  Quarter point: t=%.2f, expected=%.2f, got=%.2f\n", current_time, expected, result);
	TEST_ASSERT_DOUBLE_EQ(result, expected, EPSILON, "Linear interpolation quarter point");

	return TEST_PASS;
}

int test_interpolate_umag_boundary_conditions()
{
	// Test at time=0 and beyond last time point
	double vel_data[] = {5.0, 10.0, 15.0};
	int num_time_steps = 3;
	double dt = 1.0;

	// At t=0
	double result = interpolate_umag(vel_data, num_time_steps, 0.0, dt);
	TEST_ASSERT_DOUBLE_EQ(result, 5.0, EPSILON, "Interpolation at t=0");

	// At last time point
	double last_time = (num_time_steps - 1) * dt;
	result = interpolate_umag(vel_data, num_time_steps, last_time, dt);
	TEST_ASSERT_DOUBLE_EQ(result, 15.0, EPSILON, "Interpolation at last point");

	// Beyond last time point (should clamp or extrapolate)
	result = interpolate_umag(vel_data, num_time_steps, last_time + 1.0, dt);
	VERBOSE_PRINT("  Beyond last point: t=%.2f, result=%.2f\n", last_time + 1.0, result);

	// Should not be NaN or Inf
	TEST_ASSERT(!isnan(result) && !isinf(result), "Beyond last point should not be NaN/Inf");

	return TEST_PASS;
}

int test_interpolate_umag_negative_time()
{
	// Test with negative time (should clamp to 0 or handle gracefully)
	double vel_data[] = {100.0, 200.0};
	int num_time_steps = 2;
	double dt = 1.0;

	double result = interpolate_umag(vel_data, num_time_steps, -1.0, dt);

	VERBOSE_PRINT("  Negative time: t=-1.0, result=%.2f\n", result);
	TEST_ASSERT(!isnan(result) && !isinf(result), "Negative time should not produce NaN/Inf");

	return TEST_PASS;
}

int test_interpolate_umag_small_timestep()
{
	// Test with small timestep (high frequency data)
	double vel_data[] = {1.0, 1.1, 1.2, 1.3, 1.4};
	int num_time_steps = 5;
	double dt = 0.001; // 1 millisecond

	double current_time = 0.0015; // Between 1.1 and 1.2
	double result = interpolate_umag(vel_data, num_time_steps, current_time, dt);

	VERBOSE_PRINT("  Small timestep: t=%.4f, result=%.4f\n", current_time, result);
	TEST_ASSERT(result >= 1.0 && result <= 1.5, "Result should be within data range");
	TEST_ASSERT(!isnan(result) && !isinf(result), "Small timestep should not produce NaN/Inf");

	return TEST_PASS;
}

int test_interpolate_umag_large_array()
{
	// Test with large array (stress test)
	int num_time_steps = 10000;
	double *vel_data = (double *)malloc(num_time_steps * sizeof(double));
	TEST_ASSERT(vel_data != NULL, "Memory allocation for large array");

	// Fill with linear ramp
	for (int i = 0; i < num_time_steps; i++)
	{
		vel_data[i] = (double)i;
	}

	double dt = 0.01;
	double current_time = 50.005; // Should interpolate between 5000 and 5001

	double result = interpolate_umag(vel_data, num_time_steps, current_time, dt);

	VERBOSE_PRINT("  Large array: t=%.3f, result=%.3f\n", current_time, result);
	TEST_ASSERT(!isnan(result) && !isinf(result), "Large array should not produce NaN/Inf");
	TEST_ASSERT(result >= 5000.0 && result <= 5001.0, "Large array interpolation should be accurate");

	free(vel_data);
	return TEST_PASS;
}

int test_interpolate_umag_constant_data()
{
	// Test with constant velocity data
	double vel_data[] = {42.0, 42.0, 42.0, 42.0, 42.0};
	int num_time_steps = 5;
	double dt = 0.5;

	// Should always return 42.0 regardless of time
	for (double t = 0.0; t < 3.0; t += 0.3)
	{
		double result = interpolate_umag(vel_data, num_time_steps, t, dt);
		VERBOSE_PRINT("  Constant data: t=%.2f, result=%.2f\n", t, result);
		TEST_ASSERT_DOUBLE_EQ(result, 42.0, EPSILON, "Constant data interpolation");
	}

	return TEST_PASS;
}

int test_interpolate_umag_zero_values()
{
	// Test with zero velocities
	double vel_data[] = {0.0, 0.0, 0.0};
	int num_time_steps = 3;
	double dt = 1.0;

	double result = interpolate_umag(vel_data, num_time_steps, 0.5, dt);
	TEST_ASSERT_DOUBLE_EQ(result, 0.0, EPSILON, "Zero velocity interpolation");

	return TEST_PASS;
}

int test_interpolate_umag_alternating_values()
{
	// Test with alternating high/low values
	double vel_data[] = {0.0, 10.0, 0.0, 10.0, 0.0};
	int num_time_steps = 5;
	double dt = 1.0;

	// Midpoint between 0 and 10 should be 5
	double result = interpolate_umag(vel_data, num_time_steps, 0.5, dt);
	VERBOSE_PRINT("  Alternating: t=0.5, result=%.2f\n", result);
	TEST_ASSERT_DOUBLE_EQ(result, 5.0, EPSILON, "Alternating values midpoint");

	// Midpoint between 10 and 0 should be 5
	result = interpolate_umag(vel_data, num_time_steps, 1.5, dt);
	VERBOSE_PRINT("  Alternating: t=1.5, result=%.2f\n", result);
	TEST_ASSERT_DOUBLE_EQ(result, 5.0, EPSILON, "Alternating values midpoint (reverse)");

	return TEST_PASS;
}

// ====================================================================================
// Test Cases: get_closest_umag function
// ====================================================================================

int test_get_closest_umag_exact_match()
{
	// Test getting closest value at exact time points
	double vel_data[] = {1.0, 2.0, 3.0, 4.0, 5.0};
	int num_time_steps = 5;
	double dt = 0.1;

	for (int i = 0; i < num_time_steps; i++)
	{
		double current_time = i * dt;
		double result = get_closest_umag(vel_data, num_time_steps, current_time, dt);

		VERBOSE_PRINT("  t=%.2f: expected=%.2f, got=%.2f\n", current_time, vel_data[i], result);
		TEST_ASSERT_DOUBLE_EQ(result, vel_data[i], EPSILON, "Exact time point closest");
	}

	return TEST_PASS;
}

int test_get_closest_umag_rounding()
{
	// Test rounding behavior for closest value
	double vel_data[] = {10.0, 20.0, 30.0};
	int num_time_steps = 3;
	double dt = 1.0;

	// At t=0.4, closer to index 0 (10.0)
	double result = get_closest_umag(vel_data, num_time_steps, 0.4, dt);
	VERBOSE_PRINT("  t=0.4: result=%.2f (expected 10.0)\n", result);
	TEST_ASSERT_DOUBLE_EQ(result, 10.0, EPSILON, "Closest value rounding down");

	// At t=0.6, closer to index 1 (20.0)
	result = get_closest_umag(vel_data, num_time_steps, 0.6, dt);
	VERBOSE_PRINT("  t=0.6: result=%.2f (expected 20.0)\n", result);
	TEST_ASSERT_DOUBLE_EQ(result, 20.0, EPSILON, "Closest value rounding up");

	return TEST_PASS;
}

int test_get_closest_umag_boundary()
{
	// Test boundary conditions
	double vel_data[] = {100.0, 200.0, 300.0};
	int num_time_steps = 3;
	double dt = 1.0;

	// At t=0
	double result = get_closest_umag(vel_data, num_time_steps, 0.0, dt);
	TEST_ASSERT_DOUBLE_EQ(result, 100.0, EPSILON, "Closest at t=0");

	// Beyond last point
	result = get_closest_umag(vel_data, num_time_steps, 10.0, dt);
	VERBOSE_PRINT("  t=10.0: result=%.2f\n", result);
	TEST_ASSERT(!isnan(result) && !isinf(result), "Closest beyond end should not be NaN/Inf");

	return TEST_PASS;
}

// ====================================================================================
// Main Test Runner
// ====================================================================================

int main(int argc, char *argv[])
{
	// Check for verbose flag
	if (argc > 1 && strcmp(argv[1], "-v") == 0)
	{
		verbose_mode = 1;
	}

	printf(COLOR_CYAN "\n");
	printf("=================================================================\n");
	printf("  Flow Generation Test Suite\n");
	printf("=================================================================\n");
	printf(COLOR_RESET "\n");

	// Interpolation tests
	printf(COLOR_YELLOW "--- interpolate_umag Tests ---\n" COLOR_RESET);
	RUN_TEST(test_interpolate_umag_exact_match);
	RUN_TEST(test_interpolate_umag_linear_interpolation);
	RUN_TEST(test_interpolate_umag_boundary_conditions);
	RUN_TEST(test_interpolate_umag_negative_time);
	RUN_TEST(test_interpolate_umag_small_timestep);
	RUN_TEST(test_interpolate_umag_large_array);
	RUN_TEST(test_interpolate_umag_constant_data);
	RUN_TEST(test_interpolate_umag_zero_values);
	RUN_TEST(test_interpolate_umag_alternating_values);

	// get_closest_umag tests
	printf(COLOR_YELLOW "\n--- get_closest_umag Tests ---\n" COLOR_RESET);
	RUN_TEST(test_get_closest_umag_exact_match);
	RUN_TEST(test_get_closest_umag_rounding);
	RUN_TEST(test_get_closest_umag_boundary);

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
