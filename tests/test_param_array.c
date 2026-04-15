/**
 * @file    test_param_array.c
 * @brief   Comprehensive test suite for parameter array system
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

// Test assertions. Every test in this file has a local variable named
// `param_array` that gets allocated by add_param_*() and must be freed before
// any early-return on failure -- otherwise the leaks accumulate across tests
// in the same process. The _PA suffix means "param_array aware": the macro
// frees param_array before returning.
#define TEST_ASSERT_PA(condition, msg)                         \
	do                                                         \
	{                                                          \
		if (!(condition))                                      \
		{                                                      \
			printf(COLOR_RED "  FAIL: %s\n" COLOR_RESET, msg); \
			tests_failed++;                                    \
			free_param_array(&param_array);                    \
			return 0;                                          \
		}                                                      \
	} while (0)

#define TEST_ASSERT_DOUBLE_EQ_PA(actual, expected, tolerance, msg)                                                                                         \
	do                                                                                                                                                     \
	{                                                                                                                                                      \
		if (fabs((actual) - (expected)) > (tolerance))                                                                                                     \
		{                                                                                                                                                  \
			printf(COLOR_RED "  FAIL: %s (expected %.10f, got %.10f, error %.10e)\n" COLOR_RESET, msg, (expected), (actual), fabs((actual) - (expected))); \
			tests_failed++;                                                                                                                                \
			free_param_array(&param_array);                                                                                                                \
			return 0;                                                                                                                                      \
		}                                                                                                                                                  \
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
// Test Cases: Basic Parameter Operations
// ====================================================================================

static int test_param_array_init_and_free()
{
	// Test initialization and cleanup
	param_array_t param_array;
	init_param_array(&param_array);

	// Should be able to free even with no parameters added
	free_param_array(&param_array);

	return TEST_PASS;
}

static int test_add_and_get_double()
{
	// Test adding and retrieving double parameters
	param_array_t param_array;
	init_param_array(&param_array);

	double test_value = 3.14159;
	add_param_double(&param_array, "pi", test_value, false);

	const double *retrieved = get_param_double(&param_array, "pi");
	TEST_ASSERT_PA(retrieved != NULL, "Double parameter should be found");
	TEST_ASSERT_DOUBLE_EQ_PA(*retrieved, test_value, EPSILON, "Double value should match");

	VERBOSE_PRINT("  Added: %.6f, Retrieved: %.6f\n", test_value, *retrieved);

	free_param_array(&param_array);
	return TEST_PASS;
}

static int test_add_and_get_int()
{
	// Test adding and retrieving int parameters
	param_array_t param_array;
	init_param_array(&param_array);

	int test_value = 42;
	add_param_int(&param_array, "answer", test_value, false);

	const int *retrieved = get_param_int(&param_array, "answer");
	TEST_ASSERT_PA(retrieved != NULL, "Int parameter should be found");
	TEST_ASSERT_PA(*retrieved == test_value, "Int value should match");

	VERBOSE_PRINT("  Added: %d, Retrieved: %d\n", test_value, *retrieved);

	free_param_array(&param_array);
	return TEST_PASS;
}

static int test_add_and_get_string()
{
	// Test adding and retrieving string parameters
	param_array_t param_array;
	init_param_array(&param_array);

	const char *test_value = "hello_world";
	add_param_string(&param_array, "greeting", test_value, false);

	const char *retrieved = NULL;
	get_param(&param_array, "greeting", &retrieved);

	TEST_ASSERT_PA(retrieved != NULL, "String parameter should be found");
	TEST_ASSERT_PA(strcmp(retrieved, test_value) == 0, "String value should match");

	VERBOSE_PRINT("  Added: '%s', Retrieved: '%s'\n", test_value, retrieved);

	free_param_array(&param_array);
	return TEST_PASS;
}

static int test_get_nonexistent_parameter()
{
	// Test retrieving a parameter that doesn't exist
	param_array_t param_array;
	init_param_array(&param_array);

	add_param_double(&param_array, "existing", 1.0, false);

	const double *retrieved = get_param_double(&param_array, "nonexistent");
	TEST_ASSERT_PA(retrieved == NULL, "Nonexistent parameter should return NULL");

	free_param_array(&param_array);
	return TEST_PASS;
}

static int test_add_multiple_parameters()
{
	// Test adding multiple parameters of different types
	param_array_t param_array;
	init_param_array(&param_array);

	double dt = 0.01;
	double duration = 10.0;
	int num_steps = 1000;
	const char *filename = "output.csv";

	add_param_double(&param_array, "dt", dt, false);
	add_param_double(&param_array, "duration", duration, false);
	add_param_int(&param_array, "num_steps", num_steps, false);
	add_param_string(&param_array, "filename", filename, false);

	// Retrieve and verify all
	const double *dt_ptr = get_param_double(&param_array, "dt");
	const double *dur_ptr = get_param_double(&param_array, "duration");
	const int *steps_ptr = get_param_int(&param_array, "num_steps");
	const char *file_ptr = NULL;
	get_param(&param_array, "filename", &file_ptr);

	TEST_ASSERT_PA(dt_ptr != NULL && fabs(*dt_ptr - dt) < EPSILON, "dt parameter correct");
	TEST_ASSERT_PA(dur_ptr != NULL && fabs(*dur_ptr - duration) < EPSILON, "duration parameter correct");
	TEST_ASSERT_PA(steps_ptr != NULL && *steps_ptr == num_steps, "num_steps parameter correct");
	TEST_ASSERT_PA(file_ptr != NULL && strcmp(file_ptr, filename) == 0, "filename parameter correct");

	free_param_array(&param_array);
	return TEST_PASS;
}

// ====================================================================================
// Test Cases: State Variables
// ====================================================================================

static int test_state_variable_marking()
{
	// Test marking parameters as state variables vs fixed
	param_array_t param_array;
	init_param_array(&param_array);

	// State variables (dynamic)
	double position = 0.0;
	double velocity = 1.0;

	// Fixed parameters
	double mass = 10.0;
	double dt = 0.01;

	add_param_double(&param_array, "position", position, true); // state var
	add_param_double(&param_array, "velocity", velocity, true); // state var
	add_param_double(&param_array, "mass", mass, false);        // fixed
	add_param_double(&param_array, "dt", dt, false);            // fixed

	// All should be retrievable
	const double *pos_ptr = get_param_double(&param_array, "position");
	const double *vel_ptr = get_param_double(&param_array, "velocity");
	const double *mass_ptr = get_param_double(&param_array, "mass");
	const double *dt_ptr = get_param_double(&param_array, "dt");

	TEST_ASSERT_PA(pos_ptr != NULL, "position found");
	TEST_ASSERT_PA(vel_ptr != NULL, "velocity found");
	TEST_ASSERT_PA(mass_ptr != NULL, "mass found");
	TEST_ASSERT_PA(dt_ptr != NULL, "dt found");

	free_param_array(&param_array);
	return TEST_PASS;
}

static int test_state_variable_update()
{
	// Test that state variables can be updated through pointers
	param_array_t param_array;
	init_param_array(&param_array);

	double time = 0.0;
	add_param_double(&param_array, "time", time, true);

	// Get pointer
	double *time_ptr = NULL;
	get_param(&param_array, "time", &time_ptr);
	TEST_ASSERT_PA(time_ptr != NULL, "time pointer retrieved");

	// Update through pointer
	*time_ptr = 1.5;

	// Retrieve again and verify
	const double *time_ptr2 = get_param_double(&param_array, "time");
	TEST_ASSERT_DOUBLE_EQ_PA(*time_ptr2, 1.5, EPSILON, "State variable updated correctly");

	VERBOSE_PRINT("  Updated time to: %.2f\n", *time_ptr2);

	free_param_array(&param_array);
	return TEST_PASS;
}

// ====================================================================================
// Test Cases: Edge Cases and Error Handling
// ====================================================================================

static int test_empty_parameter_array()
{
	// Test operations on empty parameter array
	param_array_t param_array;
	init_param_array(&param_array);

	const double *ptr = get_param_double(&param_array, "anything");
	TEST_ASSERT_PA(ptr == NULL, "Empty array should return NULL for any parameter");

	free_param_array(&param_array);
	return TEST_PASS;
}

static int test_duplicate_parameter_names()
{
	// Test adding parameters with duplicate names (should update or error)
	param_array_t param_array;
	init_param_array(&param_array);

	add_param_double(&param_array, "value", 1.0, false);
	add_param_double(&param_array, "value", 2.0, false); // Duplicate name

	// The behavior depends on implementation - either last wins or first wins
	const double *ptr = get_param_double(&param_array, "value");
	TEST_ASSERT_PA(ptr != NULL, "Parameter with duplicate name should still be accessible");

	VERBOSE_PRINT("  Duplicate name result: %.2f\n", *ptr);

	free_param_array(&param_array);
	return TEST_PASS;
}

static int test_parameter_with_empty_name()
{
	// Test adding parameter with empty name
	param_array_t param_array;
	init_param_array(&param_array);

	add_param_double(&param_array, "", 1.0, false);

	const double *ptr = get_param_double(&param_array, "");
	// May or may not work depending on implementation

	VERBOSE_PRINT("  Empty name parameter: %s\n", ptr != NULL ? "accessible" : "not accessible");

	free_param_array(&param_array);
	return TEST_PASS;
}

static int test_large_number_of_parameters()
{
	// Stress test with many parameters
	param_array_t param_array;
	init_param_array(&param_array);

	const int num_params = 1000;
	for (int i = 0; i < num_params; i++)
	{
		char name[64];
		snprintf(name, sizeof(name), "param_%d", i);
		add_param_double(&param_array, name, (double)i, false);
	}

	// Verify a few
	const double *p0 = get_param_double(&param_array, "param_0");
	const double *p500 = get_param_double(&param_array, "param_500");
	const double *p999 = get_param_double(&param_array, "param_999");

	TEST_ASSERT_PA(p0 != NULL && fabs(*p0 - 0.0) < EPSILON, "First parameter correct");
	TEST_ASSERT_PA(p500 != NULL && fabs(*p500 - 500.0) < EPSILON, "Middle parameter correct");
	TEST_ASSERT_PA(p999 != NULL && fabs(*p999 - 999.0) < EPSILON, "Last parameter correct");

	VERBOSE_PRINT("  Added %d parameters successfully\n", num_params);

	free_param_array(&param_array);
	return TEST_PASS;
}

static int test_very_long_parameter_name()
{
	// Test with very long parameter name
	param_array_t param_array;
	init_param_array(&param_array);

	char long_name[256];
	memset(long_name, 'a', sizeof(long_name) - 1);
	long_name[sizeof(long_name) - 1] = '\0';

	add_param_double(&param_array, long_name, 42.0, false);

	const double *ptr = get_param_double(&param_array, long_name);
	TEST_ASSERT_PA(ptr != NULL, "Long parameter name should be accessible");
	TEST_ASSERT_DOUBLE_EQ_PA(*ptr, 42.0, EPSILON, "Long name parameter value correct");

	free_param_array(&param_array);
	return TEST_PASS;
}

static int test_special_characters_in_name()
{
	// Test parameter names with special characters
	param_array_t param_array;
	init_param_array(&param_array);

	add_param_double(&param_array, "param-with-dashes", 1.0, false);
	add_param_double(&param_array, "param_with_underscores", 2.0, false);
	add_param_double(&param_array, "param.with.dots", 3.0, false);
	add_param_double(&param_array, "param123", 4.0, false);

	const double *p1 = get_param_double(&param_array, "param-with-dashes");
	const double *p2 = get_param_double(&param_array, "param_with_underscores");
	const double *p3 = get_param_double(&param_array, "param.with.dots");
	const double *p4 = get_param_double(&param_array, "param123");

	TEST_ASSERT_PA(p1 != NULL && fabs(*p1 - 1.0) < EPSILON, "Dashes in name");
	TEST_ASSERT_PA(p2 != NULL && fabs(*p2 - 2.0) < EPSILON, "Underscores in name");
	TEST_ASSERT_PA(p3 != NULL && fabs(*p3 - 3.0) < EPSILON, "Dots in name");
	TEST_ASSERT_PA(p4 != NULL && fabs(*p4 - 4.0) < EPSILON, "Numbers in name");

	free_param_array(&param_array);
	return TEST_PASS;
}

// ====================================================================================
// Test Cases: Extreme Values
// ====================================================================================

static int test_extreme_double_values()
{
	// Test with very large and very small double values
	param_array_t param_array;
	init_param_array(&param_array);

	double very_large = 1.0e308;
	double very_small = 1.0e-308;
	double negative_large = -1.0e308;
	double zero = 0.0;

	add_param_double(&param_array, "very_large", very_large, false);
	add_param_double(&param_array, "very_small", very_small, false);
	add_param_double(&param_array, "negative_large", negative_large, false);
	add_param_double(&param_array, "zero", zero, false);

	const double *p1 = get_param_double(&param_array, "very_large");
	const double *p2 = get_param_double(&param_array, "very_small");
	const double *p3 = get_param_double(&param_array, "negative_large");
	const double *p4 = get_param_double(&param_array, "zero");

	TEST_ASSERT_PA(p1 != NULL && fabs(*p1 - very_large) < very_large * 1e-10, "Very large value");
	TEST_ASSERT_PA(p2 != NULL && fabs(*p2 - very_small) < very_small * 1e-10, "Very small value");
	TEST_ASSERT_PA(p3 != NULL && fabs(*p3 - negative_large) < fabs(negative_large) * 1e-10, "Negative large value");
	TEST_ASSERT_PA(p4 != NULL && fabs(*p4 - zero) < EPSILON, "Zero value");

	free_param_array(&param_array);
	return TEST_PASS;
}

static int test_nan_and_inf_values()
{
	// Test with NaN and infinity
	param_array_t param_array;
	init_param_array(&param_array);

	double nan_value = NAN;
	double inf_value = INFINITY;
	double neg_inf_value = -INFINITY;

	add_param_double(&param_array, "nan", nan_value, false);
	add_param_double(&param_array, "inf", inf_value, false);
	add_param_double(&param_array, "neg_inf", neg_inf_value, false);

	const double *p1 = get_param_double(&param_array, "nan");
	const double *p2 = get_param_double(&param_array, "inf");
	const double *p3 = get_param_double(&param_array, "neg_inf");

	TEST_ASSERT_PA(p1 != NULL && isnan(*p1), "NaN value preserved");
	TEST_ASSERT_PA(p2 != NULL && isinf(*p2) && *p2 > 0, "Infinity value preserved");
	TEST_ASSERT_PA(p3 != NULL && isinf(*p3) && *p3 < 0, "Negative infinity value preserved");

	VERBOSE_PRINT("  NaN/Inf values handled correctly\n");

	free_param_array(&param_array);
	return TEST_PASS;
}

static int test_extreme_int_values()
{
	// Test with extreme integer values
	param_array_t param_array;
	init_param_array(&param_array);

	int max_int = 2147483647;  // INT_MAX
	int min_int = -2147483648; // INT_MIN (approximately)
	int zero = 0;

	add_param_int(&param_array, "max_int", max_int, false);
	add_param_int(&param_array, "min_int", min_int, false);
	add_param_int(&param_array, "zero", zero, false);

	const int *p1 = get_param_int(&param_array, "max_int");
	const int *p2 = get_param_int(&param_array, "min_int");
	const int *p3 = get_param_int(&param_array, "zero");

	TEST_ASSERT_PA(p1 != NULL && *p1 == max_int, "Max int value");
	TEST_ASSERT_PA(p2 != NULL && *p2 == min_int, "Min int value");
	TEST_ASSERT_PA(p3 != NULL && *p3 == zero, "Zero int value");

	free_param_array(&param_array);
	return TEST_PASS;
}

// ====================================================================================
// Test Cases: Type Safety
// ====================================================================================

static int test_type_mismatch_double_as_int()
{
	// Test retrieving double parameter as int (type mismatch)
	param_array_t param_array;
	init_param_array(&param_array);

	add_param_double(&param_array, "value", 3.14, false);

	// Try to retrieve as int
	const int *ptr = get_param_int(&param_array, "value");

	// Behavior depends on implementation - may return NULL or cast
	VERBOSE_PRINT("  Type mismatch (double as int): %s\n", ptr != NULL ? "allowed" : "prevented");

	free_param_array(&param_array);
	return TEST_PASS;
}

static int test_type_mismatch_int_as_double()
{
	// Test retrieving int parameter as double (type mismatch)
	param_array_t param_array;
	init_param_array(&param_array);

	add_param_int(&param_array, "value", 42, false);

	// Try to retrieve as double
	const double *ptr = get_param_double(&param_array, "value");

	VERBOSE_PRINT("  Type mismatch (int as double): %s\n", ptr != NULL ? "allowed" : "prevented");

	free_param_array(&param_array);
	return TEST_PASS;
}

// ====================================================================================
// Test Cases: Memory Safety
// ====================================================================================

static int test_multiple_init_free_cycles()
{
	// Test multiple init/free cycles
	param_array_t param_array;

	for (int i = 0; i < 10; i++)
	{
		init_param_array(&param_array);
		add_param_double(&param_array, "value", (double)i, false);

		const double *ptr = get_param_double(&param_array, "value");
		TEST_ASSERT_PA(ptr != NULL && fabs(*ptr - (double)i) < EPSILON, "Init/free cycle correct");

		free_param_array(&param_array);
	}

	VERBOSE_PRINT("  Completed 10 init/free cycles\n");

	return TEST_PASS;
}

static int test_parameter_persistence()
{
	// Test that parameters persist after retrieval
	param_array_t param_array;
	init_param_array(&param_array);

	add_param_double(&param_array, "value", 1.0, false);

	// Retrieve multiple times
	const double *ptr1 = get_param_double(&param_array, "value");
	const double *ptr2 = get_param_double(&param_array, "value");
	const double *ptr3 = get_param_double(&param_array, "value");

	TEST_ASSERT_PA(ptr1 != NULL, "First retrieval succeeds");
	TEST_ASSERT_PA(ptr2 != NULL, "Second retrieval succeeds");
	TEST_ASSERT_PA(ptr3 != NULL, "Third retrieval succeeds");

	// All should point to same value
	TEST_ASSERT_DOUBLE_EQ_PA(*ptr1, *ptr2, EPSILON, "Retrievals point to same value");
	TEST_ASSERT_DOUBLE_EQ_PA(*ptr2, *ptr3, EPSILON, "Retrievals point to same value");

	free_param_array(&param_array);
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
	printf("  Parameter Array Test Suite\n");
	printf("=================================================================\n");
	printf(COLOR_RESET "\n");

	// Basic operations
	printf(COLOR_YELLOW "--- Basic Operations ---\n" COLOR_RESET);
	RUN_TEST(test_param_array_init_and_free);
	RUN_TEST(test_add_and_get_double);
	RUN_TEST(test_add_and_get_int);
	RUN_TEST(test_add_and_get_string);
	RUN_TEST(test_get_nonexistent_parameter);
	RUN_TEST(test_add_multiple_parameters);

	// State variables
	printf(COLOR_YELLOW "\n--- State Variables ---\n" COLOR_RESET);
	RUN_TEST(test_state_variable_marking);
	RUN_TEST(test_state_variable_update);

	// Edge cases
	printf(COLOR_YELLOW "\n--- Edge Cases ---\n" COLOR_RESET);
	RUN_TEST(test_empty_parameter_array);
	RUN_TEST(test_duplicate_parameter_names);
	RUN_TEST(test_parameter_with_empty_name);
	RUN_TEST(test_large_number_of_parameters);
	RUN_TEST(test_very_long_parameter_name);
	RUN_TEST(test_special_characters_in_name);

	// Extreme values
	printf(COLOR_YELLOW "\n--- Extreme Values ---\n" COLOR_RESET);
	RUN_TEST(test_extreme_double_values);
	RUN_TEST(test_nan_and_inf_values);
	RUN_TEST(test_extreme_int_values);

	// Type safety
	printf(COLOR_YELLOW "\n--- Type Safety ---\n" COLOR_RESET);
	RUN_TEST(test_type_mismatch_double_as_int);
	RUN_TEST(test_type_mismatch_int_as_double);

	// Memory safety
	printf(COLOR_YELLOW "\n--- Memory Safety ---\n" COLOR_RESET);
	RUN_TEST(test_multiple_init_free_cycles);
	RUN_TEST(test_parameter_persistence);

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
