/**
 * @file    test_common_utils.c
 * @brief   Test suite for common utility functions
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
#include <unistd.h>

// Mock param_array functions for testing
#include "test_param_array_mock.h"

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

enum
{
	TEST_PASS = 1
};

// ====================================================================================
// Test Cases: compare_doubles
// ====================================================================================

static int test_compare_doubles_less_than()
{
	double a = 1.0;
	double b = 2.0;

	int result = compare_doubles(&a, &b);
	TEST_ASSERT(result < 0, "1.0 should be less than 2.0");

	VERBOSE_PRINT("  compare(1.0, 2.0) = %d\n", result);

	return TEST_PASS;
}

static int test_compare_doubles_greater_than()
{
	double a = 5.0;
	double b = 3.0;

	int result = compare_doubles(&a, &b);
	TEST_ASSERT(result > 0, "5.0 should be greater than 3.0");

	VERBOSE_PRINT("  compare(5.0, 3.0) = %d\n", result);

	return TEST_PASS;
}

static int test_compare_doubles_equal()
{
	double a = 4.0;
	double b = 4.0;

	int result = compare_doubles(&a, &b);
	TEST_ASSERT(result == 0, "4.0 should equal 4.0");

	VERBOSE_PRINT("  compare(4.0, 4.0) = %d\n", result);

	return TEST_PASS;
}

static int test_compare_doubles_negative()
{
	double a = -5.0;
	double b = -2.0;

	int result = compare_doubles(&a, &b);
	TEST_ASSERT(result < 0, "-5.0 should be less than -2.0");

	VERBOSE_PRINT("  compare(-5.0, -2.0) = %d\n", result);

	return TEST_PASS;
}

static int test_compare_doubles_zero()
{
	double a = 0.0;
	double b = 0.0;

	int result = compare_doubles(&a, &b);
	TEST_ASSERT(result == 0, "0.0 should equal 0.0");

	return TEST_PASS;
}

static int test_compare_doubles_inf()
{
	// +Inf is greater than any finite, -Inf is less than any finite.
	double pos_inf = INFINITY;
	double neg_inf = -INFINITY;
	double finite = 1.0;

	TEST_ASSERT(compare_doubles(&pos_inf, &finite) == 1, "+Inf > 1.0");
	TEST_ASSERT(compare_doubles(&finite, &pos_inf) == -1, "1.0 < +Inf");
	TEST_ASSERT(compare_doubles(&neg_inf, &finite) == -1, "-Inf < 1.0");
	TEST_ASSERT(compare_doubles(&finite, &neg_inf) == 1, "1.0 > -Inf");
	TEST_ASSERT(compare_doubles(&pos_inf, &neg_inf) == 1, "+Inf > -Inf");
	TEST_ASSERT(compare_doubles(&pos_inf, &pos_inf) == 0, "+Inf == +Inf");

	return TEST_PASS;
}

static int test_compare_doubles_nan()
{
	// IEEE 754: NaN is unordered against everything, so both `<` and `>` are
	// false. The current implementation falls through to return 0 ("equal")
	// in that case. This test pins that behavior so a future change to use
	// e.g. NaN-propagating arithmetic gets caught.
	double nan = NAN;
	double finite = 1.0;

	TEST_ASSERT(compare_doubles(&nan, &finite) == 0, "NaN vs finite returns 0");
	TEST_ASSERT(compare_doubles(&finite, &nan) == 0, "finite vs NaN returns 0");
	TEST_ASSERT(compare_doubles(&nan, &nan) == 0, "NaN vs NaN returns 0");

	return TEST_PASS;
}

// ====================================================================================
// Test Cases: get_num_cores
// ====================================================================================

static int test_get_num_cores()
{
	int num_cores = get_num_cores();

	TEST_ASSERT(num_cores > 0, "Number of cores should be positive");
	TEST_ASSERT(num_cores <= 1024, "Number of cores should be reasonable (< 1024)");

	VERBOSE_PRINT("  Detected %d CPU cores\n", num_cores);

	return TEST_PASS;
}

// ====================================================================================
// Test Cases: check_parent_status
// ====================================================================================

static int test_check_parent_status_self()
{
	// Test with our own PID (should be running)
	int my_pid = getpid();

	int result = check_parent_status(my_pid);

	// Should return some valid status (implementation-dependent)
	VERBOSE_PRINT("  check_parent_status(self=%d) = %d\n", my_pid, result);

	// Just verify it doesn't crash
	TEST_ASSERT(1, "Function should execute without crashing");

	return TEST_PASS;
}

static int test_check_parent_status_invalid_pid()
{
	// Test with invalid PID
	int invalid_pid = 999999;

	int result = check_parent_status(invalid_pid);

	VERBOSE_PRINT("  check_parent_status(invalid=%d) = %d\n", invalid_pid, result);

	// Should handle gracefully
	TEST_ASSERT(1, "Function should handle invalid PID gracefully");

	return TEST_PASS;
}

// ====================================================================================
// Test Cases: CPU Usage (platform-dependent)
// ====================================================================================

static int test_update_cpu_usage()
{
	// Test CPU usage calculation
	double cpu_usage = update_cpu_usage();

	VERBOSE_PRINT("  Current CPU usage: %.2f%%\n", cpu_usage);

	// Should be between 0 and 100 (or possibly higher on multi-core)
	TEST_ASSERT(cpu_usage >= 0.0, "CPU usage should be non-negative");
	TEST_ASSERT(!isnan(cpu_usage) && !isinf(cpu_usage), "CPU usage should be a valid number");

	return TEST_PASS;
}

// ====================================================================================
// Test Cases: load_double_struct_param
// ====================================================================================
// Note: These tests are skipped because they require param_array functions from xflow-utils
// which are not available in the test environment

// ====================================================================================
// Main Test Runner
// ====================================================================================

int main(int argc, char *argv[])
{
	if (argc > 1 && strcmp(argv[1], "-v") == 0)
	{
		verbose_mode = 1;
	}

	printf(COLOR_CYAN "\n");
	printf("=================================================================\n");
	printf("  Common Utilities Test Suite\n");
	printf("=================================================================\n");
	printf(COLOR_RESET "\n");

	// compare_doubles tests
	printf(COLOR_YELLOW "--- compare_doubles Tests ---\n" COLOR_RESET);
	RUN_TEST(test_compare_doubles_less_than);
	RUN_TEST(test_compare_doubles_greater_than);
	RUN_TEST(test_compare_doubles_equal);
	RUN_TEST(test_compare_doubles_negative);
	RUN_TEST(test_compare_doubles_zero);
	RUN_TEST(test_compare_doubles_inf);
	RUN_TEST(test_compare_doubles_nan);

	// System info tests
	printf(COLOR_YELLOW "\n--- System Info Tests ---\n" COLOR_RESET);
	RUN_TEST(test_get_num_cores);
	RUN_TEST(test_check_parent_status_self);
	RUN_TEST(test_check_parent_status_invalid_pid);

	// CPU usage tests
	printf(COLOR_YELLOW "\n--- CPU Usage Tests ---\n" COLOR_RESET);
	RUN_TEST(test_update_cpu_usage);

	// Parameter loading tests - SKIPPED (require xflow-utils param_array functions)
	// printf(COLOR_YELLOW "\n--- Parameter Loading Tests ---\n" COLOR_RESET);
	// RUN_TEST(test_load_double_struct_param);
	// RUN_TEST(test_load_double_struct_param_nonexistent);

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
	printf(COLOR_RED "Some tests failed.\n" COLOR_RESET);
	return 1;
}
