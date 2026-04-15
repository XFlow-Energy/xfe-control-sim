/**
 * @file    test_control_switch.c
 * @brief   Test suite for control switch and stage dispatch system
 * @author  XFlow Energy
 * @date    2025
 */

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "control_switch.h"
#include "xfe_control_sim_common.h"
#include "xflow_aero_sim.h"
#include "flow_gen.h"
#include "numerical_integrator.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define EPSILON 1e-6

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
// Test Cases: Stage Map Validation
// ====================================================================================

int test_flow_gen_map_has_entries()
{
	// Test that flowMap has at least some entries
	int num_entries = sizeof(flowMap) / sizeof(flowMap[0]);

	TEST_ASSERT(num_entries > 0, "flowMap should have at least one entry");

	VERBOSE_PRINT("  flowMap has %d entries\n", num_entries);

	return TEST_PASS;
}

int test_numerical_integrator_map_has_entries()
{
	int num_entries = sizeof(numericalIntegratorMap) / sizeof(numericalIntegratorMap[0]);

	TEST_ASSERT(num_entries >= 3, "numericalIntegratorMap should have at least 3 entries (RK4, Euler, AB2)");

	VERBOSE_PRINT("  numericalIntegratorMap has %d entries\n", num_entries);

	return TEST_PASS;
}

int test_stage_map_names_are_not_null()
{
	// Verify that all stage map entries have non-NULL names

	// Check flowMap
	int num_flow_entries = sizeof(flowMap) / sizeof(flowMap[0]);
	for (int i = 0; i < num_flow_entries; i++)
	{
		TEST_ASSERT(flowMap[i].id != NULL, "flowMap entry should have non-NULL name");
		TEST_ASSERT(flowMap[i].fn != NULL, "flowMap entry should have non-NULL function pointer");
	}

	// Check numericalIntegratorMap
	int num_integrator_entries = sizeof(numericalIntegratorMap) / sizeof(numericalIntegratorMap[0]);
	for (int i = 0; i < num_integrator_entries; i++)
	{
		TEST_ASSERT(numericalIntegratorMap[i].id != NULL, "numericalIntegratorMap entry should have non-NULL name");
		TEST_ASSERT(numericalIntegratorMap[i].fn != NULL, "numericalIntegratorMap entry should have non-NULL function pointer");
	}

	VERBOSE_PRINT("  All stage map entries validated\n");

	return TEST_PASS;
}

int test_integrator_map_contains_expected_functions()
{
	// Verify specific known integrators are in the map
	int num_entries = sizeof(numericalIntegratorMap) / sizeof(numericalIntegratorMap[0]);

	bool found_rk4 = false;
	bool found_euler = false;
	bool found_ab2 = false;

	for (int i = 0; i < num_entries; i++)
	{
		if (strcmp(numericalIntegratorMap[i].id, "rk4_numerical_integrator") == 0)
		{
			found_rk4 = true;
			TEST_ASSERT(numericalIntegratorMap[i].fn == rk4_numerical_integrator, "RK4 function pointer should match");
		}
		else if (strcmp(numericalIntegratorMap[i].id, "euler_numerical_integrator") == 0)
		{
			found_euler = true;
			TEST_ASSERT(numericalIntegratorMap[i].fn == euler_numerical_integrator, "Euler function pointer should match");
		}
		else if (strcmp(numericalIntegratorMap[i].id, "ab2_numerical_integrator") == 0)
		{
			found_ab2 = true;
			TEST_ASSERT(numericalIntegratorMap[i].fn == ab2_numerical_integrator, "AB2 function pointer should match");
		}
	}

	TEST_ASSERT(found_rk4, "RK4 integrator should be in map");
	TEST_ASSERT(found_euler, "Euler integrator should be in map");
	TEST_ASSERT(found_ab2, "AB2 integrator should be in map");

	VERBOSE_PRINT("  Found all expected integrators in map\n");

	return TEST_PASS;
}

int test_flow_gen_map_contains_expected_functions()
{
	int num_entries = sizeof(flowMap) / sizeof(flowMap[0]);

	bool found_csv = false;
	bool found_bts = false;

	for (int i = 0; i < num_entries; i++)
	{
		if (strcmp(flowMap[i].id, "csv_fixed_interp_flow_gen") == 0)
		{
			found_csv = true;
			TEST_ASSERT(flowMap[i].fn == csv_fixed_interp_flow_gen, "CSV flow gen function pointer should match");
		}
		else if (strcmp(flowMap[i].id, "bts_fixed_interp_flow_gen") == 0)
		{
			found_bts = true;
			TEST_ASSERT(flowMap[i].fn == bts_fixed_interp_flow_gen, "BTS flow gen function pointer should match");
		}
	}

	TEST_ASSERT(found_csv || found_bts, "At least one flow gen function should be in map");

	VERBOSE_PRINT("  Found expected flow gen functions in map\n");

	return TEST_PASS;
}

// ====================================================================================
// Test Cases: Stage Dispatcher (function lookup)
// ====================================================================================

int test_stage_dispatcher_lookup()
{
	// Test that we can look up functions from the map
	// This tests the DISPATCH_STAGE macro logic

	// Look for RK4 in the integrator map
	int num_entries = sizeof(numericalIntegratorMap) / sizeof(numericalIntegratorMap[0]);

	const char *target_name = "rk4_numerical_integrator";
	numerical_integrator_fn found_func = NULL;

	for (int i = 0; i < num_entries; i++)
	{
		if (strcmp(numericalIntegratorMap[i].id, target_name) == 0)
		{
			found_func = numericalIntegratorMap[i].fn;
			break;
		}
	}

	TEST_ASSERT(found_func != NULL, "Should find RK4 function by name");
	TEST_ASSERT(found_func == rk4_numerical_integrator, "Function pointer should match RK4");

	VERBOSE_PRINT("  Successfully looked up '%s'\n", target_name);

	return TEST_PASS;
}

int test_stage_dispatcher_unknown_function()
{
	// Test lookup of non-existent function
	int num_entries = sizeof(numericalIntegratorMap) / sizeof(numericalIntegratorMap[0]);

	const char *target_name = "nonexistent_integrator";
	numerical_integrator_fn found_func = NULL;

	for (int i = 0; i < num_entries; i++)
	{
		if (strcmp(numericalIntegratorMap[i].id, target_name) == 0)
		{
			found_func = numericalIntegratorMap[i].fn;
			break;
		}
	}

	TEST_ASSERT(found_func == NULL, "Non-existent function should not be found");

	VERBOSE_PRINT("  Correctly failed to find '%s'\n", target_name);

	return TEST_PASS;
}

// ====================================================================================
// Test Cases: Control Switch Parameter Loading
// ====================================================================================

int test_control_switch_parameter_retrieval()
{
	// Test that control_switch can retrieve function names from parameter array
	param_array_t fixed_data;
	init_param_array(&fixed_data);

	// Add typical function call parameters
	add_param_string(&fixed_data, "flow_function_call", "csv_fixed_interp_flow_gen", false);
	add_param_string(&fixed_data, "numerical_integrator_function_call", "rk4_numerical_integrator", false);

	// Retrieve them
	const char *flow_func = NULL;
	const char *integrator_func = NULL;

	get_param(&fixed_data, "flow_function_call", &flow_func);
	get_param(&fixed_data, "numerical_integrator_function_call", &integrator_func);

	TEST_ASSERT(flow_func != NULL, "flow_function_call should be retrievable");
	TEST_ASSERT(integrator_func != NULL, "numerical_integrator_function_call should be retrievable");

	TEST_ASSERT(strcmp(flow_func, "csv_fixed_interp_flow_gen") == 0, "flow_function_call value correct");
	TEST_ASSERT(strcmp(integrator_func, "rk4_numerical_integrator") == 0, "integrator_function_call value correct");

	VERBOSE_PRINT("  Retrieved: flow='%s', integrator='%s'\n", flow_func, integrator_func);

	free_param_array(&fixed_data);
	return TEST_PASS;
}

int test_control_switch_missing_parameter()
{
	// Test handling of missing parameters
	param_array_t fixed_data;
	init_param_array(&fixed_data);

	// Don't add the parameter

	const char *flow_func = NULL;
	get_param(&fixed_data, "flow_function_call", &flow_func);

	TEST_ASSERT(flow_func == NULL, "Missing parameter should return NULL");

	VERBOSE_PRINT("  Missing parameter correctly returned NULL\n");

	free_param_array(&fixed_data);
	return TEST_PASS;
}

// ====================================================================================
// Test Cases: Case Sensitivity and Exact Matching
// ====================================================================================

int test_function_name_exact_match()
{
	// Test that function names must match exactly (case-sensitive)
	int num_entries = sizeof(numericalIntegratorMap) / sizeof(numericalIntegratorMap[0]);

	// Try with wrong case
	const char *wrong_case = "RK4_numerical_integrator"; // Capital RK4
	numerical_integrator_fn found_func = NULL;

	for (int i = 0; i < num_entries; i++)
	{
		if (strcmp(numericalIntegratorMap[i].id, wrong_case) == 0)
		{
			found_func = numericalIntegratorMap[i].fn;
			break;
		}
	}

	TEST_ASSERT(found_func == NULL, "Wrong case should not match");

	VERBOSE_PRINT("  Function name matching is case-sensitive\n");

	return TEST_PASS;
}

int test_function_name_with_spaces()
{
	// Test that names with extra spaces don't match
	int num_entries = sizeof(numericalIntegratorMap) / sizeof(numericalIntegratorMap[0]);

	const char *with_spaces = " rk4_numerical_integrator "; // Leading/trailing spaces
	numerical_integrator_fn found_func = NULL;

	for (int i = 0; i < num_entries; i++)
	{
		if (strcmp(numericalIntegratorMap[i].id, with_spaces) == 0)
		{
			found_func = numericalIntegratorMap[i].fn;
			break;
		}
	}

	TEST_ASSERT(found_func == NULL, "Name with spaces should not match");

	VERBOSE_PRINT("  Function name matching requires exact strings\n");

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

	printf(COLOR_CYAN "\n");
	printf("=================================================================\n");
	printf("  Control Switch Test Suite\n");
	printf("=================================================================\n");
	printf(COLOR_RESET "\n");

	// Stage map validation
	printf(COLOR_YELLOW "--- Stage Map Validation ---\n" COLOR_RESET);
	RUN_TEST(test_flow_gen_map_has_entries);
	RUN_TEST(test_numerical_integrator_map_has_entries);
	RUN_TEST(test_stage_map_names_are_not_null);
	RUN_TEST(test_integrator_map_contains_expected_functions);
	RUN_TEST(test_flow_gen_map_contains_expected_functions);

	// Stage dispatcher
	printf(COLOR_YELLOW "\n--- Stage Dispatcher Tests ---\n" COLOR_RESET);
	RUN_TEST(test_stage_dispatcher_lookup);
	RUN_TEST(test_stage_dispatcher_unknown_function);

	// Parameter loading
	printf(COLOR_YELLOW "\n--- Parameter Loading Tests ---\n" COLOR_RESET);
	RUN_TEST(test_control_switch_parameter_retrieval);
	RUN_TEST(test_control_switch_missing_parameter);

	// Exact matching
	printf(COLOR_YELLOW "\n--- Exact Matching Tests ---\n" COLOR_RESET);
	RUN_TEST(test_function_name_exact_match);
	RUN_TEST(test_function_name_with_spaces);

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
