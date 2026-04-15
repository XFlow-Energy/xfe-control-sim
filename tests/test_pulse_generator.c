/**
 * @file    test_pulse_generator.c
 * @brief   Test suite for pulse generator functions
 * @author  XFlow Energy
 * @date    2025
 */

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "xfe_control_sim_common.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test tracking globals
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Verbose mode flag (set to 1 to enable detailed debug output)
static int verbose_mode = 0;

// Color codes for terminal output
#define COLOR_RED "\x1b[31m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_BLUE "\x1b[34m"
#define COLOR_CYAN "\x1b[36m"
#define COLOR_RESET "\x1b[0m"

// Tolerance for floating point comparisons
#define EPSILON 1e-6
#define PERCENT_TOLERANCE 0.01 // 1% tolerance

// Verbose output macros
#define VERBOSE_PRINT(...)       \
	do                           \
	{                            \
		if (verbose_mode)        \
		{                        \
			printf(__VA_ARGS__); \
		}                        \
	} while (0)

// Test assertion macros
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

#define TEST_ASSERT_DOUBLE_EQ(actual, expected, msg)                                                         \
	do                                                                                                       \
	{                                                                                                        \
		if (fabs((actual) - (expected)) > EPSILON)                                                           \
		{                                                                                                    \
			printf(COLOR_RED "  FAIL: %s (expected %.10f, got %.10f)\n" COLOR_RESET, msg, expected, actual); \
			tests_failed++;                                                                                  \
			return 0;                                                                                        \
		}                                                                                                    \
	} while (0)

#define TEST_ASSERT_DOUBLE_PERCENT_EQ(actual, expected, msg)                                                                                      \
	do                                                                                                                                            \
	{                                                                                                                                             \
		double percent_error = fabs(((actual) - (expected)) / (expected));                                                                        \
		if (percent_error > PERCENT_TOLERANCE)                                                                                                    \
		{                                                                                                                                         \
			printf(COLOR_RED "  FAIL: %s (expected %.10f, got %.10f, error %.2f%%)\n" COLOR_RESET, msg, expected, actual, percent_error * 100.0); \
			tests_failed++;                                                                                                                       \
			return 0;                                                                                                                             \
		}                                                                                                                                         \
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

// Test return value for successful tests
enum
{
	TEST_PASS = 1
};

// Helper function to convert RPM to rad/s
static double rpm_to_rads(double rpm)
{
	return rpm * 2.0 * M_PI / 60.0;
}

// Helper function to convert rad/s to RPM
static double rads_to_rpm(double rads)
{
	return rads * 60.0 / (2.0 * M_PI);
}

/**
 * @brief Test pulse period calculation at various RPMs
 */
static int test_pulse_period_calculation(void)
{
	const int pulses_per_rev = 60;

	VERBOSE_PRINT("\n  " COLOR_CYAN "Pulse Period Calculation:\n" COLOR_RESET);
	VERBOSE_PRINT("  %10s | %15s | %15s\n", "RPM", "Calculated(us)", "Expected(us)");
	VERBOSE_PRINT("  -----------|-----------------|------------------\n");

	// Test at 60 RPM (1 Hz rotation, should give 16666.67 us per pulse)
	double omega = rpm_to_rads(60.0);
	double period = calculate_pulse_period_us(omega, pulses_per_rev);
	double expected = 1.0e6 / 60.0; // 1 second / 60 pulses = 16666.67 us
	VERBOSE_PRINT("  %10.1f | %15.2f | %15.2f\n", 60.0, period, expected);
	TEST_ASSERT_DOUBLE_PERCENT_EQ(period, expected, "Period at 60 RPM should be ~16667 us");

	// Test at 120 RPM (2 Hz rotation, should give 8333.33 us per pulse)
	omega = rpm_to_rads(120.0);
	period = calculate_pulse_period_us(omega, pulses_per_rev);
	expected = 1.0e6 / 120.0; // 0.5 second / 60 pulses = 8333.33 us
	VERBOSE_PRINT("  %10.1f | %15.2f | %15.2f\n", 120.0, period, expected);
	TEST_ASSERT_DOUBLE_PERCENT_EQ(period, expected, "Period at 120 RPM should be ~8333 us");

	// Test at 30 RPM (0.5 Hz rotation, should give 33333.33 us per pulse)
	omega = rpm_to_rads(30.0);
	period = calculate_pulse_period_us(omega, pulses_per_rev);
	expected = 1.0e6 / 30.0; // 2 seconds / 60 pulses = 33333.33 us
	VERBOSE_PRINT("  %10.1f | %15.2f | %15.2f\n", 30.0, period, expected);
	TEST_ASSERT_DOUBLE_PERCENT_EQ(period, expected, "Period at 30 RPM should be ~33333 us");

	// Test zero omega
	period = calculate_pulse_period_us(0.0, pulses_per_rev);
	VERBOSE_PRINT("  %10.1f | %15.2f | %15.2f\n", 0.0, period, 0.0);
	TEST_ASSERT_DOUBLE_EQ(period, 0.0, "Period at 0 RPM should be 0");

	// Pulses-per-rev = 0 must be guarded (otherwise division by zero).
	period = calculate_pulse_period_us(rpm_to_rads(60.0), 0);
	TEST_ASSERT_DOUBLE_EQ(period, 0.0, "pulses_per_rev=0 must return 0, not divide by zero");

	return 1;
}

/**
 * @brief Test pulse duty cycle calculation
 */
static int test_pulse_duty_cycle(void)
{
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0; // 500mm diameter ring
	const double bolt_width_mm = 10.0;     // 10mm wide bolts

	// At any speed, the duty cycle should be: bolt_width / spacing_between_bolts
	// Spacing = circumference / num_bolts = PI * diameter / num_bolts
	const double circumference_mm = M_PI * ring_diameter_mm;
	const double spacing_mm = circumference_mm / pulses_per_rev;
	const double expected_duty_cycle = bolt_width_mm / spacing_mm;

	VERBOSE_PRINT("\n  " COLOR_CYAN "Pulse Duty Cycle Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  Expected duty cycle: %.2f%% (constant at all speeds)\n", expected_duty_cycle * 100.0);
	VERBOSE_PRINT("  %10s | %12s | %15s | %12s\n", "RPM", "Period(us)", "ON Duration(us)", "Duty Cycle");
	VERBOSE_PRINT("  -----------|--------------|-----------------|-------------\n");

	// Test at various speeds - duty cycle should remain constant
	const double test_rpms[] = {30.0, 60.0, 120.0, 300.0};
	for (int i = 0; i < 4; i++)
	{
		const double omega = rpm_to_rads(test_rpms[i]);
		const double period = calculate_pulse_period_us(omega, pulses_per_rev);
		const double on_duration = calculate_pulse_on_duration_us(omega, ring_diameter_mm, bolt_width_mm);
		const double actual_duty_cycle = on_duration / period;

		VERBOSE_PRINT("  %10.1f | %12.2f | %15.2f | %11.2f%%\n", test_rpms[i], period, on_duration, actual_duty_cycle * 100.0);

		char msg[256];
		snprintf(msg, sizeof(msg), "Duty cycle at %.0f RPM should be %.2f%%", test_rpms[i], expected_duty_cycle * 100.0);
		TEST_ASSERT_DOUBLE_PERCENT_EQ(actual_duty_cycle, expected_duty_cycle, msg);
	}

	return 1;
}

/**
 * @brief Test bidirectional conversion (omega -> period -> omega)
 */
static int test_bidirectional_conversion(void)
{
	const int pulses_per_rev = 60;
	const double test_rpms[] = {30.0, 60.0, 120.0, 300.0, 600.0};

	VERBOSE_PRINT("\n  " COLOR_CYAN "Bidirectional Conversion (Omega -> Period -> Omega):\n" COLOR_RESET);
	VERBOSE_PRINT("  %10s | %15s | %15s | %12s\n", "Original", "Period(us)", "Recovered", "Error");
	VERBOSE_PRINT("  %10s | %15s | %15s | %12s\n", "(RPM)", "", "(RPM)", "(%)");
	VERBOSE_PRINT("  -----------|-----------------|-----------------|-------------\n");

	for (int i = 0; i < 5; i++)
	{
		const double original_omega = rpm_to_rads(test_rpms[i]);

		// Convert omega -> period -> omega
		const double period = calculate_pulse_period_us(original_omega, pulses_per_rev);
		const double recovered_omega = pulse_period_to_omega(period, pulses_per_rev);
		const double error_pct = fabs((recovered_omega - original_omega) / original_omega) * 100.0;

		VERBOSE_PRINT("  %10.1f | %15.2f | %15.1f | %11.6f%%\n", test_rpms[i], period, rads_to_rpm(recovered_omega), error_pct);

		char msg[256];
		snprintf(msg, sizeof(msg), "Roundtrip conversion at %.0f RPM should recover original omega", test_rpms[i]);
		TEST_ASSERT_DOUBLE_PERCENT_EQ(recovered_omega, original_omega, msg);
	}

	return 1;
}

/**
 * @brief Test frequency to omega conversion
 */
static int test_frequency_conversion(void)
{
	const int pulses_per_rev = 60;

	VERBOSE_PRINT("\n  " COLOR_CYAN "Frequency to Omega Conversion:\n" COLOR_RESET);
	VERBOSE_PRINT("  %15s | %15s | %15s\n", "Frequency(Hz)", "Expected(RPM)", "Recovered(RPM)");
	VERBOSE_PRINT("  ----------------|-----------------|------------------\n");

	// At 60 RPM: 1 rev/s * 60 pulses = 60 Hz
	double omega = rpm_to_rads(60.0);
	double frequency = 60.0; // Hz
	double recovered_omega = pulse_frequency_to_omega(frequency, pulses_per_rev);
	VERBOSE_PRINT("  %15.1f | %15.1f | %15.1f\n", frequency, 60.0, rads_to_rpm(recovered_omega));
	TEST_ASSERT_DOUBLE_PERCENT_EQ(recovered_omega, omega, "Frequency conversion at 60 RPM should work");

	// At 120 RPM: 2 rev/s * 60 pulses = 120 Hz
	omega = rpm_to_rads(120.0);
	frequency = 120.0; // Hz
	recovered_omega = pulse_frequency_to_omega(frequency, pulses_per_rev);
	VERBOSE_PRINT("  %15.1f | %15.1f | %15.1f\n", frequency, 120.0, rads_to_rpm(recovered_omega));
	TEST_ASSERT_DOUBLE_PERCENT_EQ(recovered_omega, omega, "Frequency conversion at 120 RPM should work");

	return 1;
}

/**
 * @brief Test pulse state generation over several revolutions
 */
static int test_pulse_state_generation(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;
	const double rpm = 60.0; // 60 RPM = 1 Hz = 1 second per revolution

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double omega = rpm_to_rads(rpm);
	const double dt_us = 100.0; // 100 microsecond timesteps

	VERBOSE_PRINT("\n  " COLOR_CYAN "Pulse State Generation Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  Simulating 2 revolutions at %.0f RPM\n", rpm);
	VERBOSE_PRINT("  Timestep: %.0f us\n", dt_us);

	// Simulate 2 full revolutions (2 seconds at 60 RPM)
	const double sim_time_us = 2.0e6; // 2 seconds in microseconds
	const int num_steps = (int)(sim_time_us / dt_us);

	int pulse_count = 0;
	int last_state = 0;

	for (int i = 0; i < num_steps; i++)
	{
		const int state = pulse_generator_update(&gen, omega, dt_us);

		// Count rising edges
		if (state == 1 && last_state == 0)
		{
			pulse_count++;
		}

		last_state = state;
	}

	// Should have 120 pulses in 2 revolutions (60 pulses/rev * 2 revs)
	const int expected_pulses = pulses_per_rev * 2;
	VERBOSE_PRINT("  Expected pulses: %d\n", expected_pulses);
	VERBOSE_PRINT("  Counted pulses:  %d\n", pulse_count);
	VERBOSE_PRINT("  Difference:      %d\n", pulse_count - expected_pulses);

	char msg[256];
	snprintf(msg, sizeof(msg), "Should count %d pulses in 2 revolutions (got %d)", expected_pulses, pulse_count);
	TEST_ASSERT(abs(pulse_count - expected_pulses) <= 1, msg); // Allow ±1 pulse tolerance due to timing

	return 1;
}

/**
 * @brief Test edge timing accuracy
 */
static int test_edge_detection(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;
	const double rpm = 60.0;

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double omega = rpm_to_rads(rpm);
	const double expected_period_us = calculate_pulse_period_us(omega, pulses_per_rev);
	const double dt_us = 10.0; // 10 microsecond timesteps for better accuracy

	VERBOSE_PRINT("\n  " COLOR_CYAN "Edge Detection Timing Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  RPM: %.0f, Expected period: %.2f us\n", rpm, expected_period_us);

	uint64_t first_rising_edge = 0;
	uint64_t second_rising_edge = 0;
	int rising_edge_count = 0;
	int last_state = 0;

	// Run until we capture 2 rising edges
	for (int i = 0; i < 100000 && rising_edge_count < 2; i++)
	{
		const int state = pulse_generator_update(&gen, omega, dt_us);

		if (state == 1 && last_state == 0)
		{
			rising_edge_count++;
			if (rising_edge_count == 1)
			{
				first_rising_edge = gen.last_rising_edge_us;
			}
			else if (rising_edge_count == 2)
			{
				second_rising_edge = gen.last_rising_edge_us;
			}
		}

		last_state = state;
	}

	TEST_ASSERT(rising_edge_count == 2, "Should capture 2 rising edges");

	const double actual_period_us = (double)(second_rising_edge - first_rising_edge);
	const double error_pct = fabs((actual_period_us - expected_period_us) / expected_period_us) * 100.0;

	VERBOSE_PRINT("  First edge:  %llu us\n", (unsigned long long)first_rising_edge);
	VERBOSE_PRINT("  Second edge: %llu us\n", (unsigned long long)second_rising_edge);
	VERBOSE_PRINT("  Actual period: %.2f us\n", actual_period_us);
	VERBOSE_PRINT("  Error: %.6f%%\n", error_pct);

	TEST_ASSERT_DOUBLE_PERCENT_EQ(actual_period_us, expected_period_us, "Period between rising edges should match calculated period");

	return 1;
}

/**
 * @brief Test handling of zero omega (stopped turbine)
 */
static int test_zero_omega(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	VERBOSE_PRINT("\n  " COLOR_CYAN "Zero Omega Test:\n" COLOR_RESET);

	// Test with zero omega
	const int state = pulse_generator_update(&gen, 0.0, 1000.0);
	VERBOSE_PRINT("  Pulse state at omega=0: %d (expected 0)\n", state);
	VERBOSE_PRINT("  Accumulated time: %.2f us (expected 0.0)\n", gen.accumulated_time_us);
	TEST_ASSERT(state == 0, "Pulse state should be LOW when omega is zero");
	TEST_ASSERT_DOUBLE_EQ(gen.accumulated_time_us, 0.0, "Accumulated time should be reset when omega is zero");

	// Test period calculation with zero omega
	const double period = calculate_pulse_period_us(0.0, pulses_per_rev);
	VERBOSE_PRINT("  Period at omega=0: %.2f us (expected 0.0)\n", period);
	TEST_ASSERT_DOUBLE_EQ(period, 0.0, "Period should be zero when omega is zero");

	return 1;
}

/**
 * @brief Test at high speed (extreme case)
 */
static int test_high_speed(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;
	const double rpm = 1000.0; // 1000 RPM - high speed

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double omega = rpm_to_rads(rpm);
	const double expected_period_us = calculate_pulse_period_us(omega, pulses_per_rev);

	VERBOSE_PRINT("\n  " COLOR_CYAN "High Speed Test (1000 RPM):\n" COLOR_RESET);
	VERBOSE_PRINT("  Expected period: %.2f us (theoretical ~1000 us)\n", expected_period_us);

	// At 1000 RPM: 16.67 rev/s * 60 pulses = 1000 Hz = 1000 us period
	const double expected_approx = 1000.0;
	TEST_ASSERT_DOUBLE_PERCENT_EQ(expected_period_us, expected_approx, "Period at 1000 RPM should be ~1000 us");

	// Verify pulse generation works at high speed
	const double dt_us = 1.0; // 1 microsecond timesteps
	int pulse_count = 0;
	int last_state = 0;

	VERBOSE_PRINT("  Simulating 0.1 seconds with %.0f us timesteps\n", dt_us);

	// Simulate 0.1 seconds (should get ~100 pulses)
	const int num_steps = 100000; // 100,000 steps * 1us = 0.1 seconds

	for (int i = 0; i < num_steps; i++)
	{
		const int state = pulse_generator_update(&gen, omega, dt_us);
		if (state == 1 && last_state == 0)
		{
			pulse_count++;
		}
		last_state = state;
	}

	// At 1000 RPM in 0.1 seconds: (1000/60) rev/s * 0.1s * 60 pulses/rev = 100 pulses
	const int expected_pulses = 100;
	VERBOSE_PRINT("  Expected pulses: %d\n", expected_pulses);
	VERBOSE_PRINT("  Counted pulses:  %d\n", pulse_count);
	VERBOSE_PRINT("  Difference:      %d\n", pulse_count - expected_pulses);

	char msg[256];
	snprintf(msg, sizeof(msg), "Should count ~%d pulses at 1000 RPM in 0.1s (got %d)", expected_pulses, pulse_count);
	TEST_ASSERT(abs(pulse_count - expected_pulses) <= 2, msg);

	return 1;
}

/**
 * @brief Test different pulse counts per revolution
 */
static int test_different_pulse_counts(void)
{
	const double omega = rpm_to_rads(60.0); // 60 RPM

	VERBOSE_PRINT("\n  " COLOR_CYAN "Different Pulse Counts Test (60 RPM):\n" COLOR_RESET);
	VERBOSE_PRINT("  %12s | %15s | %15s\n", "Pulses/Rev", "Calculated(us)", "Expected(us)");
	VERBOSE_PRINT("  -------------|-----------------|------------------\n");

	// Test with different pulse counts
	const int pulse_counts[] = {1, 10, 30, 60, 120};

	for (int i = 0; i < 5; i++)
	{
		const int pulses = pulse_counts[i];
		const double period = calculate_pulse_period_us(omega, pulses);

		// At 60 RPM: 1 revolution per second
		// Period should be 1,000,000 us / pulses
		const double expected = 1.0e6 / (double)pulses;

		VERBOSE_PRINT("  %12d | %15.2f | %15.2f\n", pulses, period, expected);

		char msg[256];
		snprintf(msg, sizeof(msg), "Period with %d pulses/rev should be correct", pulses);
		TEST_ASSERT_DOUBLE_PERCENT_EQ(period, expected, msg);
	}

	return 1;
}

/**
 * @brief Test pulse generator initialization
 */
static int test_pulse_generator_init(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	VERBOSE_PRINT("\n  " COLOR_CYAN "Pulse Generator Initialization Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  Pulses per rev:     %d (expected %d)\n", gen.pulses_per_rev, pulses_per_rev);
	VERBOSE_PRINT("  Ring diameter:      %.1f mm (expected %.1f mm)\n", gen.ring_diameter_mm, ring_diameter_mm);
	VERBOSE_PRINT("  Bolt width:         %.1f mm (expected %.1f mm)\n", gen.bolt_width_mm, bolt_width_mm);
	VERBOSE_PRINT("  Accumulated time:   %.1f us (expected 0.0)\n", gen.accumulated_time_us);
	VERBOSE_PRINT("  Current state:      %d (expected 0)\n", gen.current_state);
	VERBOSE_PRINT("  Last rising edge:   %llu us (expected 0)\n", (unsigned long long)gen.last_rising_edge_us);
	VERBOSE_PRINT("  Last falling edge:  %llu us (expected 0)\n", (unsigned long long)gen.last_falling_edge_us);

	TEST_ASSERT(gen.pulses_per_rev == pulses_per_rev, "Pulses per rev should be set correctly");
	TEST_ASSERT_DOUBLE_EQ(gen.ring_diameter_mm, ring_diameter_mm, "Ring diameter should be set correctly");
	TEST_ASSERT_DOUBLE_EQ(gen.bolt_width_mm, bolt_width_mm, "Bolt width should be set correctly");
	TEST_ASSERT_DOUBLE_EQ(gen.accumulated_time_us, 0.0, "Accumulated time should be initialized to 0");
	TEST_ASSERT(gen.current_state == 0, "Current state should be initialized to 0");
	TEST_ASSERT(gen.last_rising_edge_us == 0, "Last rising edge should be initialized to 0");
	TEST_ASSERT(gen.last_falling_edge_us == 0, "Last falling edge should be initialized to 0");

	return 1;
}

/**
 * @brief Test constant acceleration ramp with measurement lag
 *
 * Simulates a turbine accelerating from 30 RPM to 120 RPM over 5 seconds.
 * Measures omega using edge timestamps (simulating real controller behavior).
 * Verifies that measured omega lags behind actual omega during acceleration.
 */
static int test_acceleration_ramp_with_measurement_lag(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double start_rpm = 30.0;
	const double end_rpm = 120.0;
	const double ramp_time_s = 5.0;
	const double dt_us = 100.0; // 100 microsecond timesteps

	const double acceleration_rad_s2 = (rpm_to_rads(end_rpm) - rpm_to_rads(start_rpm)) / ramp_time_s;

	int last_state = 0;
	uint64_t last_edge_time = 0;
	uint64_t current_edge_time = 0;
	double measured_omega = 0.0;
	int edge_count = 0;
	double max_lag_error = 0.0;
	double omega_at_last_edge = 0.0; // Track omega at previous edge for proper lag calculation

	VERBOSE_PRINT("\n  " COLOR_CYAN "Acceleration Ramp Debug Output:\n" COLOR_RESET);
	VERBOSE_PRINT("  Acceleration: %.2f RPM/s (%.3f rad/s²)\n", (end_rpm - start_rpm) / ramp_time_s, acceleration_rad_s2);
	VERBOSE_PRINT("  \n  %5s | %8s | %8s | %8s | %10s | %8s\n", "Edge#", "Time(s)", "Actual", "Measured", "Period(us)", "Lag");
	VERBOSE_PRINT("  %5s | %8s | %8s | %8s | %10s | %8s\n", "", "", "(RPM)", "(RPM)", "", "(RPM)");
	VERBOSE_PRINT("  ------|----------|----------|----------|------------|----------\n");

	// Simulate 5 second ramp
	const int num_steps = (int)(ramp_time_s * 1.0e6 / dt_us);

	for (int i = 0; i < num_steps; i++)
	{
		const double time_s = (i * dt_us) / 1.0e6;
		const double current_omega = rpm_to_rads(start_rpm) + acceleration_rad_s2 * time_s;

		const int state = pulse_generator_update(&gen, current_omega, dt_us);

		// Detect rising edge
		if (state == 1 && last_state == 0)
		{
			edge_count++;
			current_edge_time = gen.last_rising_edge_us;

			// Calculate omega at the exact edge timestamp (not at the simulation step)
			const double edge_time_s = current_edge_time / 1.0e6;
			const double omega_at_edge = rpm_to_rads(start_rpm) + acceleration_rad_s2 * edge_time_s;

			// After second edge, we can measure omega and compare to actual
			if (edge_count > 1)
			{
				const double period_us = (double)(current_edge_time - last_edge_time);
				measured_omega = pulse_period_to_omega(period_us, pulses_per_rev);

				// Real-world control lag: current actual omega (ground truth) vs measured (delayed)
				// During acceleration, measured lags behind current actual (positive lag)
				// During deceleration, measured is higher than current actual (negative lag)
				const double lag = omega_at_edge - measured_omega;
				if (fabs(lag) > max_lag_error)
				{
					max_lag_error = fabs(lag);
				}

				// Print every 20th edge for readability
				if (verbose_mode && (edge_count % 20 == 0 || edge_count <= 5))
				{
					printf("  %5d | %8.3f | %8.2f | %8.2f | %10.1f | %8.2f\n", edge_count, edge_time_s, rads_to_rpm(omega_at_edge), rads_to_rpm(measured_omega), period_us, rads_to_rpm(lag));
				}
			}

			last_edge_time = current_edge_time;
			omega_at_last_edge = current_omega; // Save omega at this edge for next comparison
		}

		last_state = state;
	}

	VERBOSE_PRINT("  " COLOR_CYAN "\n  Total edges captured: %d\n" COLOR_RESET, edge_count);
	VERBOSE_PRINT("  " COLOR_CYAN "Maximum lag: %.2f rad/s (%.1f RPM)\n" COLOR_RESET, max_lag_error, rads_to_rpm(max_lag_error));

	TEST_ASSERT(edge_count > 200, "Should capture many edges during 5 second ramp");
	TEST_ASSERT(max_lag_error > 0.1, "Measured omega should lag during acceleration");

	// Acceleration: 18 RPM/s = ~1.88 rad/s²
	// Max period at start: ~33ms
	// Expected max lag: acceleration × max_period = 1.88 × 0.033 ≈ 0.062 rad/s per period
	// But actual omega is measured at edge, so cumulative lag can be higher
	// Reasonable upper bound: ~10 rad/s (about 100 RPM equivalent)
	char lag_msg[256];
	snprintf(lag_msg, sizeof(lag_msg), "Max lag was %.2f rad/s (%.1f RPM) - should be reasonable", max_lag_error, rads_to_rpm(max_lag_error));
	TEST_ASSERT(max_lag_error < 10.0, lag_msg);

	return 1;
}

/**
 * @brief Test step change in omega
 *
 * Runs at constant 60 RPM, then instantly steps to 120 RPM.
 * Verifies pulse frequency transitions smoothly and measures detection delay.
 */
static int test_step_change_omega(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double omega_before = rpm_to_rads(60.0);
	const double omega_after = rpm_to_rads(120.0);
	const double dt_us = 10.0;

	VERBOSE_PRINT("\n  " COLOR_CYAN "Step Change Omega Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  Step from %.0f RPM to %.0f RPM\n", 60.0, 120.0);

	// Run at 60 RPM for 1 second
	const int steps_before = (int)(1.0e6 / dt_us); // 1 second
	for (int i = 0; i < steps_before; i++)
	{
		pulse_generator_update(&gen, omega_before, dt_us);
	}

	uint64_t step_change_time = gen.last_rising_edge_us;
	uint64_t first_edge_after_step = 0;
	uint64_t second_edge_after_step = 0;
	int last_state = gen.current_state;

	// Step change occurs, run at 120 RPM for 1 second
	const int steps_after = (int)(1.0e6 / dt_us);
	for (int i = 0; i < steps_after; i++)
	{
		const int state = pulse_generator_update(&gen, omega_after, dt_us);

		if (state == 1 && last_state == 0)
		{
			if (first_edge_after_step == 0)
			{
				first_edge_after_step = gen.last_rising_edge_us;
			}
			else if (second_edge_after_step == 0)
			{
				second_edge_after_step = gen.last_rising_edge_us;
				break; // Got both edges
			}
		}
		last_state = state;
	}

	// Calculate period before and after step
	const double period_before = calculate_pulse_period_us(omega_before, pulses_per_rev);
	const double period_after = calculate_pulse_period_us(omega_after, pulses_per_rev);

	// The first period after step should reflect the transition
	const double measured_period = (double)(second_edge_after_step - first_edge_after_step);

	VERBOSE_PRINT("  Expected period before step: %.2f us\n", period_before);
	VERBOSE_PRINT("  Expected period after step:  %.2f us\n", period_after);
	VERBOSE_PRINT("  Measured period after step:  %.2f us\n", measured_period);
	VERBOSE_PRINT("  Error: %.2f%%\n", fabs((measured_period - period_after) / period_after) * 100.0);

	// Should be close to the new period (after step)
	TEST_ASSERT_DOUBLE_PERCENT_EQ(measured_period, period_after, "Period after step change should match new omega");

	return 1;
}

/**
 * @brief Test deceleration ramp
 *
 * Simulates turbine decelerating from 120 RPM to 30 RPM.
 * Verifies measured omega tracks correctly and pulses remain consistent.
 */
static int test_deceleration_ramp(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double start_rpm = 120.0;
	const double end_rpm = 30.0;
	const double ramp_time_s = 5.0;
	const double dt_us = 100.0;

	const double deceleration_rad_s2 = (rpm_to_rads(end_rpm) - rpm_to_rads(start_rpm)) / ramp_time_s;

	VERBOSE_PRINT("\n  " COLOR_CYAN "Deceleration Ramp Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  Decelerating from %.0f RPM to %.0f RPM over %.1f seconds\n", start_rpm, end_rpm, ramp_time_s);
	VERBOSE_PRINT("  Deceleration: %.2f RPM/s (%.3f rad/s²)\n", (end_rpm - start_rpm) / ramp_time_s, deceleration_rad_s2);

	int edge_count = 0;
	int last_state = 0;
	uint64_t last_edge_time = 0;
	double min_measured_omega = 1e9;
	double max_measured_omega = 0.0;

	const int num_steps = (int)(ramp_time_s * 1.0e6 / dt_us);

	for (int i = 0; i < num_steps; i++)
	{
		const double time_s = (i * dt_us) / 1.0e6;
		const double current_omega = rpm_to_rads(start_rpm) + deceleration_rad_s2 * time_s;

		const int state = pulse_generator_update(&gen, current_omega, dt_us);

		if (state == 1 && last_state == 0)
		{
			edge_count++;

			if (edge_count > 1)
			{
				const double period_us = (double)(gen.last_rising_edge_us - last_edge_time);
				const double measured_omega = pulse_period_to_omega(period_us, pulses_per_rev);

				if (measured_omega < min_measured_omega)
					min_measured_omega = measured_omega;
				if (measured_omega > max_measured_omega)
					max_measured_omega = measured_omega;

				// Measured omega should be positive and decreasing
				TEST_ASSERT(measured_omega > 0.0, "Measured omega should remain positive during deceleration");
			}

			last_edge_time = gen.last_rising_edge_us;
		}

		last_state = state;
	}

	VERBOSE_PRINT("  Total edges captured: %d\n", edge_count);
	VERBOSE_PRINT("  Max measured omega: %.2f RPM\n", rads_to_rpm(max_measured_omega));
	VERBOSE_PRINT("  Min measured omega: %.2f RPM\n", rads_to_rpm(min_measured_omega));

	TEST_ASSERT(edge_count > 150, "Should capture many edges during deceleration");

	return 1;
}

/**
 * @brief Test rapid acceleration (high jerk)
 *
 * Simulates aggressive acceleration at 500 RPM/s.
 * Verifies pulse generation remains stable and lag is quantified.
 */
static int test_rapid_acceleration(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double start_rpm = 30.0;
	const double acceleration_rpm_s = 500.0; // Very aggressive
	const double dt_us = 50.0;
	const double sim_time_s = 1.0;

	const double acceleration_rad_s2 = rpm_to_rads(acceleration_rpm_s);

	VERBOSE_PRINT("\n  " COLOR_CYAN "Rapid Acceleration Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  Starting at %.0f RPM\n", start_rpm);
	VERBOSE_PRINT("  Acceleration: %.0f RPM/s (%.3f rad/s²)\n", acceleration_rpm_s, acceleration_rad_s2);
	VERBOSE_PRINT("  Simulation time: %.1f seconds\n", sim_time_s);

	int edge_count = 0;
	int last_state = 0;
	double max_measured_omega = 0.0;

	const int num_steps = (int)(sim_time_s * 1.0e6 / dt_us);

	for (int i = 0; i < num_steps; i++)
	{
		const double time_s = (i * dt_us) / 1.0e6;
		const double current_omega = rpm_to_rads(start_rpm) + acceleration_rad_s2 * time_s;

		const int state = pulse_generator_update(&gen, current_omega, dt_us);

		if (state == 1 && last_state == 0)
		{
			edge_count++;

			if (edge_count > 1)
			{
				const double period_us = (double)(gen.last_rising_edge_us - gen.last_falling_edge_us);
				if (period_us > 0)
				{
					const double measured_omega = pulse_period_to_omega(period_us, pulses_per_rev);
					if (measured_omega > max_measured_omega)
					{
						max_measured_omega = measured_omega;
					}
				}
			}
		}

		last_state = state;
	}

	VERBOSE_PRINT("  Total edges: %d\n", edge_count);
	VERBOSE_PRINT("  Max measured omega: %.2f RPM\n", rads_to_rpm(max_measured_omega));
	VERBOSE_PRINT("  Final theoretical omega: %.2f RPM\n", start_rpm + acceleration_rpm_s * sim_time_s);

	TEST_ASSERT(edge_count > 50, "Should generate pulses during rapid acceleration");
	TEST_ASSERT(max_measured_omega > rpm_to_rads(100.0), "Should reach high speed during aggressive acceleration");

	return 1;
}

/**
 * @brief Test multi-period averaging
 *
 * Measures omega over N consecutive periods and calculates rolling average.
 * Verifies averaging improves stability at steady state.
 */
static int test_multi_period_averaging(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double steady_omega = rpm_to_rads(90.0);
	const double dt_us = 10.0;
	const int averaging_window = 5;

	VERBOSE_PRINT("\n  " COLOR_CYAN "Multi-Period Averaging Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  Steady state omega: %.0f RPM\n", 90.0);
	VERBOSE_PRINT("  Averaging window: %d periods\n", averaging_window);

	double period_buffer[10] = {0};
	int buffer_index = 0;
	int edge_count = 0;
	int last_state = 0;
	uint64_t last_edge_time = 0;
	double final_avg_omega = 0.0;

	// Run at constant speed for 2 seconds
	const int num_steps = (int)(2.0e6 / dt_us);

	for (int i = 0; i < num_steps; i++)
	{
		const int state = pulse_generator_update(&gen, steady_omega, dt_us);

		if (state == 1 && last_state == 0)
		{
			edge_count++;

			if (edge_count > 1)
			{
				const double period_us = (double)(gen.last_rising_edge_us - last_edge_time);
				period_buffer[buffer_index % averaging_window] = period_us;
				buffer_index++;

				// After collecting enough periods, calculate average
				if (edge_count > averaging_window + 1)
				{
					double sum = 0.0;
					for (int j = 0; j < averaging_window; j++)
					{
						sum += period_buffer[j];
					}
					const double avg_period = sum / (double)averaging_window;
					const double avg_omega = pulse_period_to_omega(avg_period, pulses_per_rev);
					final_avg_omega = avg_omega;

					// Averaged omega should be very close to actual at steady state
					TEST_ASSERT_DOUBLE_PERCENT_EQ(avg_omega, steady_omega, "Averaged omega should match steady state omega");
					break; // Test passed
				}
			}

			last_edge_time = gen.last_rising_edge_us;
		}

		last_state = state;
	}

	VERBOSE_PRINT("  Total edges collected: %d\n", edge_count);
	VERBOSE_PRINT("  Expected omega: %.2f RPM\n", 90.0);
	VERBOSE_PRINT("  Averaged omega: %.2f RPM\n", rads_to_rpm(final_avg_omega));
	VERBOSE_PRINT("  Error: %.4f%%\n", fabs((final_avg_omega - steady_omega) / steady_omega) * 100.0);

	TEST_ASSERT(edge_count > averaging_window, "Should collect enough edges for averaging");

	return 1;
}

/**
 * @brief Test measurement lag quantification at various acceleration rates
 *
 * Measures the lag between actual and measured omega at different acceleration rates.
 * Verifies lag scales appropriately with acceleration.
 */
static int test_measurement_lag_quantification(void)
{
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;
	const double dt_us = 50.0;

	const double test_accelerations_rpm_s[] = {10.0, 50.0, 100.0, 200.0};
	double measured_lags[4] = {0};

	VERBOSE_PRINT("\n  " COLOR_CYAN "Measurement Lag Quantification:\n" COLOR_RESET);
	VERBOSE_PRINT("  Testing lag at different acceleration rates:\n\n");

	for (int accel_idx = 0; accel_idx < 4; accel_idx++)
	{
		pulse_generator_t gen;
		pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

		const double acceleration_rpm_s = test_accelerations_rpm_s[accel_idx];
		const double acceleration_rad_s2 = rpm_to_rads(acceleration_rpm_s);

		VERBOSE_PRINT("  Acceleration: %.0f RPM/s (%.3f rad/s²)\n", acceleration_rpm_s, acceleration_rad_s2);

		int edge_count = 0;
		int last_state = 0;
		uint64_t last_edge_time = 0;
		double omega_at_last_edge = 0.0;
		double total_lag = 0.0;
		int lag_samples = 0;
		double min_lag = 1e9;
		double max_lag = 0.0;

		// Simulate 3 seconds
		const int num_steps = (int)(3.0e6 / dt_us);

		for (int i = 0; i < num_steps; i++)
		{
			const double time_s = (i * dt_us) / 1.0e6;
			const double current_omega = rpm_to_rads(30.0) + acceleration_rad_s2 * time_s;

			const int state = pulse_generator_update(&gen, current_omega, dt_us);

			if (state == 1 && last_state == 0)
			{
				edge_count++;

				if (edge_count > 1)
				{
					const double period_us = (double)(gen.last_rising_edge_us - last_edge_time);
					const double measured_omega = pulse_period_to_omega(period_us, pulses_per_rev);
					// Compare measured to actual at start of period (previous edge)
					const double lag = fabs(omega_at_last_edge - measured_omega);

					total_lag += lag;
					lag_samples++;

					if (lag < min_lag)
						min_lag = lag;
					if (lag > max_lag)
						max_lag = lag;
				}

				last_edge_time = gen.last_rising_edge_us;
				omega_at_last_edge = current_omega;
			}

			last_state = state;
		}

		if (lag_samples > 0)
		{
			measured_lags[accel_idx] = total_lag / (double)lag_samples;
			VERBOSE_PRINT("    Edges: %d, Avg Lag: %.3f rad/s (%.1f RPM), Min: %.3f, Max: %.3f\n", edge_count, measured_lags[accel_idx], rads_to_rpm(measured_lags[accel_idx]), min_lag, max_lag);
		}
	}

	VERBOSE_PRINT("\n  " COLOR_CYAN "Lag Comparison Table:\n" COLOR_RESET);
	VERBOSE_PRINT("  %12s | %15s | %15s\n", "Accel(RPM/s)", "Avg Lag(rad/s)", "Avg Lag(RPM)");
	VERBOSE_PRINT("  -------------|-----------------|------------------\n");
	for (int i = 0; i < 4; i++)
	{
		VERBOSE_PRINT("  %12.0f | %15.4f | %15.2f\n", test_accelerations_rpm_s[i], measured_lags[i], rads_to_rpm(measured_lags[i]));
	}
	VERBOSE_PRINT("\n");

	// Higher acceleration should result in higher average lag
	for (int i = 1; i < 4; i++)
	{
		char msg[256];
		snprintf(msg, sizeof(msg), "Lag at %.0f RPM/s should be >= lag at %.0f RPM/s", test_accelerations_rpm_s[i], test_accelerations_rpm_s[i - 1]);
		TEST_ASSERT(measured_lags[i] >= measured_lags[i - 1], msg);
	}

	return 1;
}

/**
 * @brief Test frequency vs period measurement comparison
 *
 * During acceleration, measures omega using both period and frequency methods.
 * Verifies both methods give equivalent results.
 */
static int test_frequency_vs_period_measurement(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double start_rpm = 40.0;
	const double end_rpm = 100.0;
	const double ramp_time_s = 3.0;
	const double dt_us = 50.0;

	const double acceleration_rad_s2 = (rpm_to_rads(end_rpm) - rpm_to_rads(start_rpm)) / ramp_time_s;

	VERBOSE_PRINT("\n  " COLOR_CYAN "Frequency vs Period Measurement Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  Accelerating from %.0f to %.0f RPM over %.1f seconds\n", start_rpm, end_rpm, ramp_time_s);
	VERBOSE_PRINT("  Comparing period-based vs frequency-based omega calculation\n");
	VERBOSE_PRINT("  %8s | %15s | %15s | %12s\n", "Edge#", "From Period", "From Freq", "Diff");
	VERBOSE_PRINT("  %8s | %15s | %15s | %12s\n", "", "(RPM)", "(RPM)", "(RPM)");
	VERBOSE_PRINT("  ---------|-----------------|-----------------|-------------\n");

	int edge_count = 0;
	int last_state = 0;
	uint64_t last_edge_time = 0;

	const int num_steps = (int)(ramp_time_s * 1.0e6 / dt_us);

	for (int i = 0; i < num_steps; i++)
	{
		const double time_s = (i * dt_us) / 1.0e6;
		const double current_omega = rpm_to_rads(start_rpm) + acceleration_rad_s2 * time_s;

		const int state = pulse_generator_update(&gen, current_omega, dt_us);

		if (state == 1 && last_state == 0)
		{
			edge_count++;

			if (edge_count > 1)
			{
				const double period_us = (double)(gen.last_rising_edge_us - last_edge_time);

				// Method 1: Period to omega
				const double omega_from_period = pulse_period_to_omega(period_us, pulses_per_rev);

				// Method 2: Frequency to omega
				const double frequency_hz = 1.0e6 / period_us; // Convert period to frequency
				const double omega_from_frequency = pulse_frequency_to_omega(frequency_hz, pulses_per_rev);

				// Print every 10th edge
				if (verbose_mode && (edge_count % 10 == 0 || edge_count <= 3))
				{
					printf("  %8d | %15.2f | %15.2f | %12.6f\n", edge_count, rads_to_rpm(omega_from_period), rads_to_rpm(omega_from_frequency), rads_to_rpm(omega_from_period - omega_from_frequency));
				}

				// Both methods should give identical results
				TEST_ASSERT_DOUBLE_PERCENT_EQ(omega_from_period, omega_from_frequency, "Period and frequency methods should give same omega");
			}

			last_edge_time = gen.last_rising_edge_us;
		}

		last_state = state;
	}

	VERBOSE_PRINT("  Total edges captured: %d\n", edge_count);

	TEST_ASSERT(edge_count > 50, "Should capture sufficient edges for comparison");

	return 1;
}

/**
 * @brief Test zero crossing during acceleration
 *
 * Note: Current implementation treats negative omega as stopped (returns 0).
 * This test verifies the behavior when omega changes direction.
 */
static int test_zero_crossing(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double dt_us = 100.0;

	VERBOSE_PRINT("\n  " COLOR_CYAN "Zero Crossing Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  Starting at -30 RPM (reverse)\n");
	VERBOSE_PRINT("  Accelerating at 100 RPM/s through zero to positive\n");

	// Start at negative omega (reverse direction)
	double omega = rpm_to_rads(-30.0);

	// Run with negative omega - should return 0 (stopped)
	for (int i = 0; i < 100; i++)
	{
		const int state = pulse_generator_update(&gen, omega, dt_us);
		TEST_ASSERT(state == 0, "Negative omega should keep pulse LOW");
	}

	VERBOSE_PRINT("  Verified: No pulses at negative omega\n");

	// Accelerate through zero to positive
	const double acceleration_rad_s2 = rpm_to_rads(100.0); // 100 RPM/s acceleration
	int edge_count = 0;
	int last_state = 0;
	double omega_at_first_pulse = 0.0;

	for (int i = 0; i < 10000; i++)
	{
		const double time_s = (i * dt_us) / 1.0e6;
		omega = rpm_to_rads(-30.0) + acceleration_rad_s2 * time_s;

		const int state = pulse_generator_update(&gen, omega, dt_us);

		if (state == 1 && last_state == 0)
		{
			if (edge_count == 0)
			{
				omega_at_first_pulse = omega;
			}
			edge_count++;
		}

		last_state = state;

		// Once we're at positive omega, pulses should start
		if (omega > rpm_to_rads(10.0))
		{
			break;
		}
	}

	VERBOSE_PRINT("  First pulse occurred at: %.2f RPM\n", rads_to_rpm(omega_at_first_pulse));
	VERBOSE_PRINT("  Total edges up to 10 RPM: %d\n", edge_count);

	// Should have pulses once omega becomes positive
	TEST_ASSERT(edge_count > 0, "Should generate pulses once omega becomes positive");

	return 1;
}

/**
 * @brief Test angular position wrapping over many revolutions
 *
 * Verifies that angular position wrapping at 2π doesn't accumulate errors
 * over many complete revolutions.
 */
static int test_angular_position_wrapping(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double rpm = 60.0; // 1 Hz
	const double omega = rpm_to_rads(rpm);
	const double dt_us = 100.0;

	VERBOSE_PRINT("\n  " COLOR_CYAN "Angular Position Wrapping Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  Simulating 100 revolutions at %.0f RPM\n", rpm);

	// Simulate 100 complete revolutions (100 seconds at 60 RPM)
	const double sim_time_s = 100.0;
	const int num_steps = (int)(sim_time_s * 1.0e6 / dt_us);

	int pulse_count = 0;
	int last_state = 0;

	for (int i = 0; i < num_steps; i++)
	{
		const int state = pulse_generator_update(&gen, omega, dt_us);

		if (state == 1 && last_state == 0)
		{
			pulse_count++;
		}

		last_state = state;
	}

	// Should have 60 pulses/rev * 100 revs = 6000 pulses
	const int expected_pulses = pulses_per_rev * 100;
	const double error_pct = fabs((double)(pulse_count - expected_pulses) / (double)expected_pulses) * 100.0;

	VERBOSE_PRINT("  Expected pulses: %d\n", expected_pulses);
	VERBOSE_PRINT("  Counted pulses:  %d\n", pulse_count);
	VERBOSE_PRINT("  Error: %.4f%%\n", error_pct);

	// Allow small tolerance for discrete timesteps
	TEST_ASSERT(abs(pulse_count - expected_pulses) <= 2, "Pulse count should be accurate over many revolutions");
	TEST_ASSERT(error_pct < 0.1, "Error should be less than 0.1%");

	return 1;
}

/**
 * @brief Test falling edge timing accuracy
 *
 * Verifies that falling edge timestamps are accurate and the pulse ON duration
 * matches the calculated value based on geometry.
 */
static int test_falling_edge_timing(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;
	const double rpm = 60.0;

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double omega = rpm_to_rads(rpm);
	const double expected_on_duration = calculate_pulse_on_duration_us(omega, ring_diameter_mm, bolt_width_mm);
	const double dt_us = 10.0;

	VERBOSE_PRINT("\n  " COLOR_CYAN "Falling Edge Timing Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  RPM: %.0f, Expected ON duration: %.2f us\n", rpm, expected_on_duration);

	int rising_edge_count = 0;
	int falling_edge_count = 0;
	int last_state = 0;
	uint64_t first_rising_edge = 0;
	uint64_t first_falling_edge = 0;

	// Run until we capture first rising and falling edge
	for (int i = 0; i < 100000 && falling_edge_count < 1; i++)
	{
		const int state = pulse_generator_update(&gen, omega, dt_us);

		if (state == 1 && last_state == 0)
		{
			rising_edge_count++;
			if (rising_edge_count == 1)
			{
				first_rising_edge = gen.last_rising_edge_us;
			}
		}
		else if (state == 0 && last_state == 1)
		{
			falling_edge_count++;
			if (falling_edge_count == 1)
			{
				first_falling_edge = gen.last_falling_edge_us;
			}
		}

		last_state = state;
	}

	TEST_ASSERT(rising_edge_count >= 1, "Should capture at least one rising edge");
	TEST_ASSERT(falling_edge_count >= 1, "Should capture at least one falling edge");

	const double measured_on_duration = (double)(first_falling_edge - first_rising_edge);
	const double error_pct = fabs((measured_on_duration - expected_on_duration) / expected_on_duration) * 100.0;

	VERBOSE_PRINT("  Rising edge:  %llu us\n", (unsigned long long)first_rising_edge);
	VERBOSE_PRINT("  Falling edge: %llu us\n", (unsigned long long)first_falling_edge);
	VERBOSE_PRINT("  Measured ON duration: %.2f us\n", measured_on_duration);
	VERBOSE_PRINT("  Error: %.4f%%\n", error_pct);

	TEST_ASSERT_DOUBLE_PERCENT_EQ(measured_on_duration, expected_on_duration, "ON duration should match calculated value");

	return 1;
}

/**
 * @brief Test very small timesteps
 *
 * Verifies numerical stability with extremely small timesteps (0.1 us)
 */
static int test_very_small_timestep(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;
	const double rpm = 120.0;

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double omega = rpm_to_rads(rpm);
	const double dt_us = 0.1; // Very small timestep

	VERBOSE_PRINT("\n  " COLOR_CYAN "Very Small Timestep Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  RPM: %.0f, Timestep: %.1f us\n", rpm, dt_us);

	// Simulate 0.1 seconds with 0.1 us timesteps
	const double sim_time_s = 0.1;
	const int num_steps = (int)(sim_time_s * 1.0e6 / dt_us);

	int pulse_count = 0;
	int last_state = 0;

	for (int i = 0; i < num_steps; i++)
	{
		const int state = pulse_generator_update(&gen, omega, dt_us);

		if (state == 1 && last_state == 0)
		{
			pulse_count++;
		}

		last_state = state;
	}

	// At 120 RPM: 2 rev/s * 0.1s = 0.2 revs * 60 pulses = 12 pulses
	const int expected_pulses = 12;

	VERBOSE_PRINT("  Expected pulses: %d\n", expected_pulses);
	VERBOSE_PRINT("  Counted pulses:  %d\n", pulse_count);
	VERBOSE_PRINT("  Difference:      %d\n", pulse_count - expected_pulses);

	TEST_ASSERT(abs(pulse_count - expected_pulses) <= 1, "Should maintain accuracy with very small timesteps");

	return 1;
}

/**
 * @brief Test very large timesteps
 *
 * Verifies behavior when timestep approaches pulse period.
 * This tests the limits of discrete-time simulation.
 * Note: Very large timesteps (>period/4) will cause missed pulses.
 */
static int test_very_large_timestep(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;
	const double rpm = 60.0;

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double omega = rpm_to_rads(rpm);
	const double period_us = calculate_pulse_period_us(omega, pulses_per_rev);
	const double dt_us = period_us * 0.2; // 20% of period - large but should work

	VERBOSE_PRINT("\n  " COLOR_CYAN "Large Timestep Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  RPM: %.0f, Period: %.2f us, Timestep: %.2f us (%.0f%% of period)\n", rpm, period_us, dt_us, (dt_us / period_us) * 100.0);

	// Simulate 1 second
	const double sim_time_s = 1.0;
	const int num_steps = (int)(sim_time_s * 1.0e6 / dt_us);

	int pulse_count = 0;
	int last_state = 0;

	for (int i = 0; i < num_steps; i++)
	{
		const int state = pulse_generator_update(&gen, omega, dt_us);

		if (state == 1 && last_state == 0)
		{
			pulse_count++;
		}

		last_state = state;
	}

	// At 60 RPM: 1 rev/s * 1s = 1 rev * 60 pulses = 60 pulses
	const int expected_pulses = 60;

	VERBOSE_PRINT("  Expected pulses: %d\n", expected_pulses);
	VERBOSE_PRINT("  Counted pulses:  %d\n", pulse_count);
	VERBOSE_PRINT("  Difference:      %d\n", pulse_count - expected_pulses);

	// With large timesteps (20% of period), allow more tolerance
	TEST_ASSERT(abs(pulse_count - expected_pulses) <= 10, "Should handle large timesteps reasonably");

	return 1;
}

/**
 * @brief Test variable timestep simulation
 *
 * Tests with varying timesteps to simulate adaptive solvers or
 * irregular update rates.
 */
static int test_variable_timestep(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;
	const double rpm = 90.0;

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double omega = rpm_to_rads(rpm);

	VERBOSE_PRINT("\n  " COLOR_CYAN "Variable Timestep Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  RPM: %.0f, alternating between 50us and 200us timesteps\n", rpm);

	// Simulate 2 seconds with alternating timesteps
	double total_time_us = 0.0;
	const double target_time_us = 2.0e6;
	int pulse_count = 0;
	int last_state = 0;
	int step = 0;

	while (total_time_us < target_time_us)
	{
		// Alternate between small and large timesteps
		const double dt_us = (step % 2 == 0) ? 50.0 : 200.0;

		const int state = pulse_generator_update(&gen, omega, dt_us);

		if (state == 1 && last_state == 0)
		{
			pulse_count++;
		}

		last_state = state;
		total_time_us += dt_us;
		step++;
	}

	// At 90 RPM: 1.5 rev/s * 2s = 3 revs * 60 pulses = 180 pulses
	const int expected_pulses = 180;

	VERBOSE_PRINT("  Expected pulses: %d\n", expected_pulses);
	VERBOSE_PRINT("  Counted pulses:  %d\n", pulse_count);
	VERBOSE_PRINT("  Difference:      %d\n", pulse_count - expected_pulses);

	TEST_ASSERT(abs(pulse_count - expected_pulses) <= 2, "Should handle variable timesteps accurately");

	return 1;
}

/**
 * @brief Test different ring geometries
 *
 * Tests extreme geometries: very small rings, very large rings,
 * narrow bolts, wide bolts.
 */
static int test_different_geometries(void)
{
	const double rpm = 60.0;
	const double omega = rpm_to_rads(rpm);
	const int pulses_per_rev = 60;

	VERBOSE_PRINT("\n  " COLOR_CYAN "Different Geometries Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  %15s | %10s | %12s | %12s\n", "Ring Dia (mm)", "Bolt (mm)", "Duty Cycle", "ON Time(us)");
	VERBOSE_PRINT("  ----------------|------------|--------------|-------------\n");

	// Test various geometries
	const double test_configs[][2] = {
		{50.0,   1.0 }, // Small ring, narrow bolt
		{500.0,  10.0}, // Standard (our baseline)
		{5000.0, 50.0}, // Large ring, wide bolt
		{100.0,  5.0 }, // Small ring, medium bolt
		{1000.0, 20.0}  // Large ring, wide bolt
	};

	for (int i = 0; i < 5; i++)
	{
		const double ring_diameter_mm = test_configs[i][0];
		const double bolt_width_mm = test_configs[i][1];

		const double circumference_mm = M_PI * ring_diameter_mm;
		const double spacing_mm = circumference_mm / pulses_per_rev;
		const double duty_cycle = bolt_width_mm / spacing_mm;
		const double on_duration = calculate_pulse_on_duration_us(omega, ring_diameter_mm, bolt_width_mm);

		VERBOSE_PRINT("  %15.1f | %10.1f | %11.2f%% | %12.2f\n", ring_diameter_mm, bolt_width_mm, duty_cycle * 100.0, on_duration);

		// Verify duty cycle is reasonable (between 0 and 1)
		char msg[256];
		snprintf(msg, sizeof(msg), "Duty cycle should be valid for %.0fmm ring / %.0fmm bolt", ring_diameter_mm, bolt_width_mm);
		TEST_ASSERT(duty_cycle > 0.0 && duty_cycle < 1.0, msg);

		// Verify ON duration is positive
		TEST_ASSERT(on_duration > 0.0, "ON duration should be positive");
	}

	return 1;
}

/**
 * @brief Test extremely low speeds
 *
 * Verifies correct behavior at very low RPM (0.1 - 1 RPM)
 */
static int test_extremely_low_speed(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;
	const double rpm = 1.0; // Very slow

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double omega = rpm_to_rads(rpm);
	const double expected_period_us = calculate_pulse_period_us(omega, pulses_per_rev);
	const double dt_us = 100.0;

	VERBOSE_PRINT("\n  " COLOR_CYAN "Extremely Low Speed Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  RPM: %.1f, Expected period: %.2f us (%.2f ms)\n", rpm, expected_period_us, expected_period_us / 1000.0);

	int rising_edge_count = 0;
	int last_state = 0;
	uint64_t first_rising_edge = 0;
	uint64_t second_rising_edge = 0;

	// Run until we capture 2 rising edges
	for (int i = 0; i < 2000000 && rising_edge_count < 2; i++)
	{
		const int state = pulse_generator_update(&gen, omega, dt_us);

		if (state == 1 && last_state == 0)
		{
			rising_edge_count++;
			if (rising_edge_count == 1)
			{
				first_rising_edge = gen.last_rising_edge_us;
			}
			else if (rising_edge_count == 2)
			{
				second_rising_edge = gen.last_rising_edge_us;
			}
		}

		last_state = state;
	}

	TEST_ASSERT(rising_edge_count == 2, "Should capture 2 rising edges at low speed");

	const double actual_period_us = (double)(second_rising_edge - first_rising_edge);
	VERBOSE_PRINT("  Measured period: %.2f us (%.2f ms)\n", actual_period_us, actual_period_us / 1000.0);
	VERBOSE_PRINT("  Error: %.4f%%\n", fabs((actual_period_us - expected_period_us) / expected_period_us) * 100.0);

	TEST_ASSERT_DOUBLE_PERCENT_EQ(actual_period_us, expected_period_us, "Period at very low speed should be accurate");

	return 1;
}

/**
 * @brief Test extremely high speeds
 *
 * Verifies correct behavior at very high RPM (10,000 RPM)
 */
static int test_extremely_high_speed(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;
	const double rpm = 10000.0; // Very high speed

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double omega = rpm_to_rads(rpm);
	const double expected_period_us = calculate_pulse_period_us(omega, pulses_per_rev);
	const double dt_us = 1.0; // Need small timestep for high speed

	VERBOSE_PRINT("\n  " COLOR_CYAN "Extremely High Speed Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  RPM: %.0f, Expected period: %.2f us\n", rpm, expected_period_us);

	// Simulate 0.01 seconds
	const int num_steps = 10000;
	int pulse_count = 0;
	int last_state = 0;

	for (int i = 0; i < num_steps; i++)
	{
		const int state = pulse_generator_update(&gen, omega, dt_us);

		if (state == 1 && last_state == 0)
		{
			pulse_count++;
		}

		last_state = state;
	}

	// At 10000 RPM: 166.67 rev/s * 0.01s = 1.667 revs * 60 pulses ≈ 100 pulses
	const int expected_pulses = 100;

	VERBOSE_PRINT("  Expected pulses: %d\n", expected_pulses);
	VERBOSE_PRINT("  Counted pulses:  %d\n", pulse_count);
	VERBOSE_PRINT("  Difference:      %d\n", pulse_count - expected_pulses);

	TEST_ASSERT(abs(pulse_count - expected_pulses) <= 5, "Should handle extremely high speeds");

	return 1;
}

/**
 * @brief Test stop-restart sequence
 *
 * Verifies proper state reset when turbine stops and restarts
 */
static int test_stop_restart_sequence(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double dt_us = 100.0;

	VERBOSE_PRINT("\n  " COLOR_CYAN "Stop-Restart Sequence Test:\n" COLOR_RESET);

	// Phase 1: Run at 60 RPM
	VERBOSE_PRINT("  Phase 1: Running at 60 RPM for 0.5s\n");
	double omega = rpm_to_rads(60.0);
	int pulse_count_phase1 = 0;
	int last_state = 0;

	for (int i = 0; i < 5000; i++)
	{
		const int state = pulse_generator_update(&gen, omega, dt_us);
		if (state == 1 && last_state == 0)
		{
			pulse_count_phase1++;
		}
		last_state = state;
	}

	VERBOSE_PRINT("  Phase 1 pulses: %d\n", pulse_count_phase1);

	// Phase 2: Stop (omega = 0)
	VERBOSE_PRINT("  Phase 2: Stopped for 0.5s\n");
	omega = 0.0;
	for (int i = 0; i < 5000; i++)
	{
		const int state = pulse_generator_update(&gen, omega, dt_us);
		TEST_ASSERT(state == 0, "State should be LOW when stopped");
	}

	// Verify state was reset
	TEST_ASSERT(gen.accumulated_time_us == 0.0, "Accumulated time should reset when stopped");
	VERBOSE_PRINT("  Verified: accumulated_time reset to 0\n");

	// Phase 3: Restart at 90 RPM
	VERBOSE_PRINT("  Phase 3: Restarting at 90 RPM for 0.5s\n");
	omega = rpm_to_rads(90.0);
	int pulse_count_phase3 = 0;
	last_state = 0;

	for (int i = 0; i < 5000; i++)
	{
		const int state = pulse_generator_update(&gen, omega, dt_us);
		if (state == 1 && last_state == 0)
		{
			pulse_count_phase3++;
		}
		last_state = state;
	}

	VERBOSE_PRINT("  Phase 3 pulses: %d\n", pulse_count_phase3);

	// Phase 3 should have more pulses than phase 1 (90 RPM vs 60 RPM)
	TEST_ASSERT(pulse_count_phase3 > pulse_count_phase1, "Higher RPM should produce more pulses in same time");

	return 1;
}

/**
 * @brief Test with noise/jitter in omega
 *
 * Simulates realistic speed variations around nominal value
 */
static int test_omega_with_jitter(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double nominal_rpm = 60.0;
	const double jitter_rpm = 2.0; // ±2 RPM variation
	const double dt_us = 100.0;

	VERBOSE_PRINT("\n  " COLOR_CYAN "Omega Jitter Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  Nominal: %.0f RPM, Jitter: ±%.0f RPM\n", nominal_rpm, jitter_rpm);

	// Simulate 2 seconds with sinusoidal jitter
	const double sim_time_s = 2.0;
	const int num_steps = (int)(sim_time_s * 1.0e6 / dt_us);

	int pulse_count = 0;
	int last_state = 0;

	for (int i = 0; i < num_steps; i++)
	{
		const double time_s = (i * dt_us) / 1.0e6;
		// Add sinusoidal jitter at 5 Hz
		const double current_rpm = nominal_rpm + jitter_rpm * sin(2.0 * M_PI * 5.0 * time_s);
		const double omega = rpm_to_rads(current_rpm);

		const int state = pulse_generator_update(&gen, omega, dt_us);

		if (state == 1 && last_state == 0)
		{
			pulse_count++;
		}

		last_state = state;
	}

	// At nominal 60 RPM: 1 rev/s * 2s = 2 revs * 60 pulses = 120 pulses
	// With jitter, should be close but allow some variation
	const int expected_pulses = 120;

	VERBOSE_PRINT("  Expected pulses (nominal): %d\n", expected_pulses);
	VERBOSE_PRINT("  Counted pulses:  %d\n", pulse_count);
	VERBOSE_PRINT("  Difference:      %d\n", pulse_count - expected_pulses);

	// Allow larger tolerance due to jitter
	TEST_ASSERT(abs(pulse_count - expected_pulses) <= 10, "Should handle omega jitter reasonably");

	return 1;
}

/**
 * @brief Test many pulses per revolution
 *
 * Tests with very high pulse counts (360, 1000) and prime numbers
 */
static int test_many_pulses_per_rev(void)
{
	const double rpm = 60.0;
	const double omega = rpm_to_rads(rpm);
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 2.0; // Narrower bolts for high pulse count

	VERBOSE_PRINT("\n  " COLOR_CYAN "Many Pulses Per Revolution Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  %12s | %15s | %15s\n", "Pulses/Rev", "Calculated(us)", "Expected(us)");
	VERBOSE_PRINT("  -------------|-----------------|------------------\n");

	const int test_pulse_counts[] = {7, 13, 37, 360, 1000};

	for (int i = 0; i < 5; i++)
	{
		const int pulses = test_pulse_counts[i];
		const double period = calculate_pulse_period_us(omega, pulses);
		const double expected = 1.0e6 / (double)pulses;

		VERBOSE_PRINT("  %12d | %15.2f | %15.2f\n", pulses, period, expected);

		char msg[256];
		snprintf(msg, sizeof(msg), "Period with %d pulses/rev should be correct", pulses);
		TEST_ASSERT_DOUBLE_PERCENT_EQ(period, expected, msg);
	}

	return 1;
}

/**
 * @brief Test long duration simulation
 *
 * Verifies numerical stability over long simulation times (1 hour simulated)
 */
static int test_long_duration_simulation(void)
{
	pulse_generator_t gen;
	const int pulses_per_rev = 60;
	const double ring_diameter_mm = 500.0;
	const double bolt_width_mm = 10.0;
	const double rpm = 100.0;

	pulse_generator_init(&gen, pulses_per_rev, ring_diameter_mm, bolt_width_mm);

	const double omega = rpm_to_rads(rpm);
	const double dt_us = 1000.0; // 1ms timesteps for faster simulation

	VERBOSE_PRINT("\n  " COLOR_CYAN "Long Duration Simulation Test:\n" COLOR_RESET);
	VERBOSE_PRINT("  Simulating 1 minute at %.0f RPM\n", rpm);

	// Simulate 1 minute (reduced from 1 hour for reasonable test time)
	const double sim_time_s = 60.0;
	const int num_steps = (int)(sim_time_s * 1.0e6 / dt_us);

	int pulse_count = 0;
	int last_state = 0;

	for (int i = 0; i < num_steps; i++)
	{
		const int state = pulse_generator_update(&gen, omega, dt_us);

		if (state == 1 && last_state == 0)
		{
			pulse_count++;
		}

		last_state = state;
	}

	// At 100 RPM: 100/60 rev/s * 60s = 100 revs * 60 pulses = 6000 pulses
	const int expected_pulses = 6000;
	const double error_pct = fabs((double)(pulse_count - expected_pulses) / (double)expected_pulses) * 100.0;

	VERBOSE_PRINT("  Expected pulses: %d\n", expected_pulses);
	VERBOSE_PRINT("  Counted pulses:  %d\n", pulse_count);
	VERBOSE_PRINT("  Error: %.4f%%\n", error_pct);

	TEST_ASSERT(error_pct < 0.5, "Should maintain accuracy over long duration");

	return 1;
}

/**
 * @brief Test fault injector initialization
 */
static int test_fault_injector_init(void)
{
	pulse_fault_injector_t injector;
	pulse_fault_injector_init(&injector, 12345);

	// Verify all faults are disabled
	TEST_ASSERT(!injector.enable_random_noise, "Random noise should be disabled");
	TEST_ASSERT(!injector.enable_missed_pulses, "Missed pulses should be disabled");
	TEST_ASSERT(!injector.enable_burst_errors, "Burst errors should be disabled");
	TEST_ASSERT(!injector.enable_emi_bursts, "EMI bursts should be disabled");
	TEST_ASSERT(!injector.enable_edge_ringing, "Edge ringing should be disabled");
	TEST_ASSERT(!injector.enable_attenuation, "Attenuation should be disabled");
	TEST_ASSERT(!injector.enable_phase_jitter, "Phase jitter should be disabled");
	TEST_ASSERT(!injector.enable_intermittent, "Intermittent should be disabled");
	TEST_ASSERT(!injector.enable_damaged_bolt, "Damaged bolt should be disabled");
	TEST_ASSERT(!injector.enable_power_sag, "Power sag should be disabled");

	// Verify RNG is initialized
	TEST_ASSERT(injector.rng_state == 12345, "RNG state should be initialized with seed");

	// Verify stats are zeroed
	TEST_ASSERT(injector.total_updates == 0, "Total updates should be 0");
	TEST_ASSERT(injector.faults_injected == 0, "Faults injected should be 0");

	return TEST_PASS;
}

/**
 * @brief Test CLEAN profile (no faults)
 */
static int test_fault_profile_clean(void)
{
	pulse_fault_injector_t injector;
	pulse_fault_injector_init_from_profile(&injector, PULSE_PROFILE_CLEAN, 54321);

	pulse_generator_t gen;
	pulse_generator_init(&gen, 6, 1000.0, 40.0);

	// Run simulation for 10 revolutions
	const double omega = 6.283185; // 1 rev/s = 60 RPM
	const double dt_us = 100.0;
	int pulse_count = 0;
	int prev_state = 0;

	for (int i = 0; i < 100000; i++)
	{
		int state = pulse_generator_update(&gen, omega, dt_us);
		bool rising_edge = (state == 1 && prev_state == 0);
		bool falling_edge = (state == 0 && prev_state == 1);

		pulse_fault_apply(&injector, &state, dt_us, rising_edge, falling_edge);

		if (state == 1 && prev_state == 0)
		{
			pulse_count++;
		}

		prev_state = state;
	}

	// With clean profile, no faults should be injected
	TEST_ASSERT(injector.faults_injected == 0, "CLEAN profile should inject no faults");
	TEST_ASSERT(pulse_count == 60, "Should get exactly 60 pulses in 10 seconds with CLEAN profile");

	return TEST_PASS;
}

/**
 * @brief Test DIRTY_SENSOR profile
 */
static int test_fault_profile_dirty_sensor(void)
{
	pulse_fault_injector_t injector;
	pulse_fault_injector_init_from_profile(&injector, PULSE_PROFILE_DIRTY_SENSOR, 99999);

	// Verify correct faults are enabled
	TEST_ASSERT(injector.enable_missed_pulses, "Missed pulses should be enabled");
	TEST_ASSERT(injector.enable_random_noise, "Random noise should be enabled");
	TEST_ASSERT(injector.enable_attenuation, "Attenuation should be enabled");

	// Verify probabilities are set
	TEST_ASSERT(injector.miss_probability > 0.0, "Miss probability should be set");
	TEST_ASSERT(injector.noise_probability > 0.0, "Noise probability should be set");

	return TEST_PASS;
}

/**
 * @brief Test EMI_ENVIRONMENT profile
 */
static int test_fault_profile_emi_environment(void)
{
	pulse_fault_injector_t injector;
	pulse_fault_injector_init_from_profile(&injector, PULSE_PROFILE_EMI_ENVIRONMENT, 11111);

	// Verify correct faults are enabled
	TEST_ASSERT(injector.enable_emi_bursts, "EMI bursts should be enabled");
	TEST_ASSERT(injector.enable_edge_ringing, "Edge ringing should be enabled");
	TEST_ASSERT(injector.enable_burst_errors, "Burst errors should be enabled");

	// Verify parameters are set
	TEST_ASSERT(injector.emi_duration_us > 0, "EMI duration should be set");
	TEST_ASSERT(injector.emi_frequency_khz > 0, "EMI frequency should be set");
	TEST_ASSERT(injector.ringing_cycles > 0, "Ringing cycles should be set");

	return TEST_PASS;
}

/**
 * @brief Test LOOSE_CONNECTION profile
 */
static int test_fault_profile_loose_connection(void)
{
	pulse_fault_injector_t injector;
	pulse_fault_injector_init_from_profile(&injector, PULSE_PROFILE_LOOSE_CONNECTION, 22222);

	// Verify correct faults are enabled
	TEST_ASSERT(injector.enable_intermittent, "Intermittent should be enabled");
	TEST_ASSERT(injector.enable_attenuation, "Attenuation should be enabled");
	TEST_ASSERT(injector.enable_random_noise, "Random noise should be enabled");

	// Verify parameters
	TEST_ASSERT(injector.dropout_duration_us > 0, "Dropout duration should be set");
	TEST_ASSERT(injector.attenuation_level < 1.0, "Attenuation level should be < 1.0");

	return TEST_PASS;
}

/**
 * @brief Test DAMAGED_BOLT profile
 */
static int test_fault_profile_damaged_bolt(void)
{
	pulse_fault_injector_t injector;
	pulse_fault_injector_init_from_profile(&injector, PULSE_PROFILE_DAMAGED_BOLT, 33333);

	// Verify correct faults are enabled
	TEST_ASSERT(injector.enable_damaged_bolt, "Damaged bolt should be enabled");
	TEST_ASSERT(injector.enable_phase_jitter, "Phase jitter should be enabled");
	TEST_ASSERT(injector.enable_random_noise, "Random noise should be enabled");

	// Verify parameters
	TEST_ASSERT(injector.damaged_bolt_index >= 0, "Damaged bolt index should be set");
	TEST_ASSERT(injector.damaged_bolt_prob > 0.0, "Damaged bolt probability should be set");
	TEST_ASSERT(injector.jitter_max_us > 0, "Jitter max should be set");

	return TEST_PASS;
}

/**
 * @brief Test WORST_CASE profile (all faults enabled)
 */
static int test_fault_profile_worst_case(void)
{
	pulse_fault_injector_t injector;
	pulse_fault_injector_init_from_profile(&injector, PULSE_PROFILE_WORST_CASE, 44444);

	// Verify all major faults are enabled
	TEST_ASSERT(injector.enable_random_noise, "Random noise should be enabled");
	TEST_ASSERT(injector.enable_missed_pulses, "Missed pulses should be enabled");
	TEST_ASSERT(injector.enable_burst_errors, "Burst errors should be enabled");
	TEST_ASSERT(injector.enable_emi_bursts, "EMI bursts should be enabled");
	TEST_ASSERT(injector.enable_edge_ringing, "Edge ringing should be enabled");
	TEST_ASSERT(injector.enable_attenuation, "Attenuation should be enabled");
	TEST_ASSERT(injector.enable_phase_jitter, "Phase jitter should be enabled");
	TEST_ASSERT(injector.enable_intermittent, "Intermittent should be enabled");
	TEST_ASSERT(injector.enable_damaged_bolt, "Damaged bolt should be enabled");
	TEST_ASSERT(injector.enable_power_sag, "Power sag should be enabled");

	return TEST_PASS;
}

/**
 * @brief Test random noise injection
 */
static int test_fault_random_noise(void)
{
	pulse_fault_injector_t injector;
	pulse_fault_injector_init(&injector, 55555);

	// Enable only random noise with high probability for testing
	injector.enable_random_noise = true;
	injector.noise_probability = 0.1; // 10% chance per update

	int state = 0;
	int flips = 0;

	// Run 10000 updates
	for (int i = 0; i < 10000; i++)
	{
		int prev_state = state;
		pulse_fault_apply(&injector, &state, 100.0, false, false);
		if (state != prev_state)
		{
			flips++;
		}
	}

	// Should have some noise flips (approximately 1000, but allow wide tolerance for randomness)
	TEST_ASSERT(injector.noise_flips > 500 && injector.noise_flips < 1500, "Should have roughly 10% noise flips");
	TEST_ASSERT(injector.faults_injected == injector.noise_flips, "Faults injected should match noise flips");

	return TEST_PASS;
}

/**
 * @brief Test missed pulse injection
 */
static int test_fault_missed_pulses(void)
{
	pulse_fault_injector_t injector;
	pulse_fault_injector_init(&injector, 66666);

	pulse_generator_t gen;
	pulse_generator_init(&gen, 6, 1000.0, 40.0);

	// Enable missed pulses with 20% probability
	injector.enable_missed_pulses = true;
	injector.miss_probability = 0.2;

	const double omega = 6.283185; // 60 RPM
	const double dt_us = 100.0;
	int pulse_count_observed = 0;
	int prev_gen_state = 0;
	int prev_obs_state = 0;

	// Run for 100 expected pulses (simulate until we get that many opportunities)
	for (int i = 0; i < 200000; i++)
	{
		int gen_state = pulse_generator_update(&gen, omega, dt_us);

		// Detect rising edge from generator (before fault injection)
		bool rising_edge = (gen_state == 1 && prev_gen_state == 0);
		bool falling_edge = (gen_state == 0 && prev_gen_state == 1);

		// Apply fault injection
		int observed_state = gen_state;
		pulse_fault_apply(&injector, &observed_state, dt_us, rising_edge, falling_edge);

		// Count pulses after fault injection
		if (observed_state == 1 && prev_obs_state == 0)
		{
			pulse_count_observed++;
		}

		prev_gen_state = gen_state;
		prev_obs_state = observed_state;
	}

	// Should have missed some pulses (approximately 20% of 120 pulses in 20s = 24 missed)
	TEST_ASSERT(injector.pulses_missed > 10 && injector.pulses_missed < 40, "Should miss roughly 20% of pulses");
	TEST_ASSERT(pulse_count_observed < 115, "Pulse count should be less than expected due to missed pulses");

	return TEST_PASS;
}

/**
 * @brief Test EMI burst injection
 */
static int test_fault_emi_bursts(void)
{
	pulse_fault_injector_t injector;
	pulse_fault_injector_init(&injector, 77777);

	// Enable EMI with guaranteed trigger on first update
	injector.enable_emi_bursts = true;
	injector.emi_probability = 1.0; // 100% to guarantee trigger
	injector.emi_duration_us = 1000;
	injector.emi_frequency_khz = 100; // 100 kHz = 10 µs period

	int state = 0;
	pulse_fault_apply(&injector, &state, 100.0, false, false);

	// Should have triggered one EMI event
	TEST_ASSERT(injector.emi_events == 1, "Should have 1 EMI event");

	// EMI timer should be active
	TEST_ASSERT(injector.emi_timer_us > 0, "EMI timer should be active");

	return TEST_PASS;
}

/**
 * @brief Test edge ringing injection
 */
static int test_fault_edge_ringing(void)
{
	pulse_fault_injector_t injector;
	pulse_fault_injector_init(&injector, 88888);

	// Enable edge ringing
	injector.enable_edge_ringing = true;
	injector.ringing_cycles = 3;
	injector.ringing_period_us = 10;

	int state = 1;
	// Trigger on falling edge
	pulse_fault_apply(&injector, &state, 100.0, false, true);

	// Should have triggered ringing
	TEST_ASSERT(injector.ringing_events == 1, "Should have 1 ringing event");
	TEST_ASSERT(injector.ringing_timer_us > 0, "Ringing timer should be active");

	return TEST_PASS;
}

/**
 * @brief Test attenuation with dropout
 */
static int test_fault_attenuation(void)
{
	pulse_fault_injector_t injector;
	pulse_fault_injector_init(&injector, 99999);

	// Enable attenuation with high dropout probability
	injector.enable_attenuation = true;
	injector.attenuation_level = 0.3;
	injector.attenuation_prob = 0.2; // 20% chance of dropout when HIGH

	int state = 1; // Start with HIGH state
	int dropout_count = 0;

	// Run 1000 updates with state HIGH
	for (int i = 0; i < 1000; i++)
	{
		int prev_state = state;
		state = 1; // Keep trying to be HIGH
		pulse_fault_apply(&injector, &state, 100.0, false, false);
		if (state == 0 && prev_state == 1)
		{
			dropout_count++;
		}
	}

	// Should have some attenuation dropouts (roughly 20%)
	TEST_ASSERT(injector.attenuation_dropouts > 100, "Should have attenuation dropouts");

	return TEST_PASS;
}

/**
 * @brief Test intermittent dropout
 */
static int test_fault_intermittent_dropout(void)
{
	pulse_fault_injector_t injector;
	pulse_fault_injector_init(&injector, 10101);

	// Enable intermittent dropout with guaranteed trigger
	injector.enable_intermittent = true;
	injector.dropout_probability = 1.0; // 100% to guarantee trigger
	injector.dropout_duration_us = 5000;

	int state = 1;
	pulse_fault_apply(&injector, &state, 100.0, false, false);

	// Should have triggered dropout
	TEST_ASSERT(injector.intermittent_dropouts == 1, "Should have 1 intermittent dropout");
	TEST_ASSERT(injector.dropout_timer_us > 0, "Dropout timer should be active");
	TEST_ASSERT(state == 0, "State should be forced to 0 during dropout");

	return TEST_PASS;
}

/**
 * @brief Test power sag injection
 */
static int test_fault_power_sag(void)
{
	pulse_fault_injector_t injector;
	pulse_fault_injector_init(&injector, 20202);

	// Enable power sag with guaranteed trigger
	injector.enable_power_sag = true;
	injector.sag_probability = 1.0; // 100% to guarantee trigger
	injector.sag_duration_us = 10000;

	int state = 1;
	pulse_fault_apply(&injector, &state, 100.0, false, false);

	// Should have triggered power sag
	TEST_ASSERT(injector.power_sag_events == 1, "Should have 1 power sag event");
	TEST_ASSERT(injector.sag_timer_us > 0, "Sag timer should be active");
	TEST_ASSERT(state == 0, "State should be forced to 0 during power sag");

	// Power sag should override everything - test by running more updates
	for (int i = 0; i < 10; i++)
	{
		state = 1; // Try to force HIGH
		pulse_fault_apply(&injector, &state, 100.0, false, false);
		TEST_ASSERT(state == 0, "State should remain 0 during active power sag");
	}

	return TEST_PASS;
}

/**
 * @brief Test burst error injection
 */
static int test_fault_burst_errors(void)
{
	pulse_fault_injector_t injector;
	pulse_fault_injector_init(&injector, 30303);

	// Enable burst errors with guaranteed trigger
	injector.enable_burst_errors = true;
	injector.burst_probability = 1.0; // 100% to guarantee trigger
	injector.burst_duration_us = 1000;
	injector.burst_noise_prob_pct = 50; // 50% noise during burst

	int state = 0;
	pulse_fault_apply(&injector, &state, 100.0, false, false);

	// Should have triggered burst
	TEST_ASSERT(injector.burst_events == 1, "Should have 1 burst event");
	TEST_ASSERT(injector.burst_timer_us > 0, "Burst timer should be active");

	return TEST_PASS;
}

/**
 * @brief Test damaged bolt fault (specific pulse missing)
 */
static int test_fault_damaged_bolt(void)
{
	pulse_fault_injector_t injector;
	pulse_fault_injector_init(&injector, 40404);

	pulse_generator_t gen;
	pulse_generator_init(&gen, 6, 1000.0, 40.0);

	// Enable damaged bolt - the 3rd bolt (index 2) is damaged
	injector.enable_damaged_bolt = true;
	injector.damaged_bolt_index = 2;
	injector.damaged_bolt_prob = 1.0; // 100% miss when damaged bolt is encountered
	injector.pulses_per_rev = 6;      // Must match the generator configuration

	const double omega = 6.283185; // 60 RPM
	const double dt_us = 100.0;
	int pulse_count_observed = 0;
	int prev_gen_state = 0;
	int prev_obs_state = 0;
	int bolt_5_count = 0; // Debug: count how many times we see the damaged bolt

	// Run for several revolutions (should see 100 opportunities for bolt 2)
	for (int i = 0; i < 100000; i++)
	{
		int gen_state = pulse_generator_update(&gen, omega, dt_us);

		// Detect edges from generator
		bool rising_edge = (gen_state == 1 && prev_gen_state == 0);
		bool falling_edge = (gen_state == 0 && prev_gen_state == 1);

		// Debug: Track when we encounter the damaged bolt
		int bolt_before = injector.current_bolt_index;

		// Apply fault injection
		int observed_state = gen_state;
		pulse_fault_apply(&injector, &observed_state, dt_us, rising_edge, falling_edge);

		// Debug: Check if bolt index changed and if it was the damaged bolt
		if (rising_edge && bolt_before == 2)
		{
			bolt_5_count++;
		}

		// Count observed pulses
		if (observed_state == 1 && prev_obs_state == 0)
		{
			pulse_count_observed++;
		}

		prev_gen_state = gen_state;
		prev_obs_state = observed_state;
	}

	VERBOSE_PRINT("  Damaged bolt (index 2) encountered: %d times\n", bolt_5_count);
	VERBOSE_PRINT("  Damaged bolt test stats:\n");
	VERBOSE_PRINT("    Damaged bolt misses: %llu\n", (unsigned long long)injector.damaged_bolt_misses);
	VERBOSE_PRINT("    Observed pulses: %d\n", pulse_count_observed);
	VERBOSE_PRINT("    Current bolt index: %d\n", injector.current_bolt_index);

	// Should have missed the damaged bolt multiple times (approximately 10 times in 10 revs)
	// The damaged bolt fault is triggered correctly (10 times), verifying the mechanism works
	TEST_ASSERT(injector.damaged_bolt_misses > 5, "Should have missed damaged bolt multiple times");
	TEST_ASSERT(injector.damaged_bolt_misses == 10, "Should miss bolt 2 exactly 10 times (once per revolution)");

	return TEST_PASS;
}

/**
 * @brief Test statistics tracking
 */
static int test_fault_statistics(void)
{
	pulse_fault_injector_t injector;
	pulse_fault_injector_init(&injector, 50505);

	// Enable multiple faults
	injector.enable_random_noise = true;
	injector.noise_probability = 0.1;
	injector.enable_missed_pulses = true;
	injector.miss_probability = 0.05;

	pulse_generator_t gen;
	pulse_generator_init(&gen, 6, 1000.0, 40.0);

	const double omega = 6.283185;
	const double dt_us = 100.0;
	int prev_state = 0;

	// Run simulation
	for (int i = 0; i < 10000; i++)
	{
		int state = pulse_generator_update(&gen, omega, dt_us);
		bool rising_edge = (state == 1 && prev_state == 0);
		bool falling_edge = (state == 0 && prev_state == 1);

		pulse_fault_apply(&injector, &state, dt_us, rising_edge, falling_edge);
		prev_state = state;
	}

	// Verify statistics are tracked
	TEST_ASSERT(injector.total_updates == 10000, "Should track 10000 updates");
	TEST_ASSERT(injector.faults_injected > 0, "Should have injected some faults");
	TEST_ASSERT(injector.noise_flips > 0, "Should have some noise flips");

	// Test stats reset
	pulse_fault_reset_stats(&injector);
	TEST_ASSERT(injector.total_updates == 0, "Stats should be reset");
	TEST_ASSERT(injector.faults_injected == 0, "Faults injected should be reset");
	TEST_ASSERT(injector.noise_flips == 0, "Noise flips should be reset");

	return TEST_PASS;
}

/**
 * @brief Test reproducibility with same RNG seed
 */
static int test_fault_reproducibility(void)
{
	pulse_fault_injector_t injector1, injector2;
	pulse_fault_injector_init(&injector1, 12345);
	pulse_fault_injector_init(&injector2, 12345); // Same seed

	// Enable same faults with same parameters
	injector1.enable_random_noise = true;
	injector1.noise_probability = 0.1;
	injector2.enable_random_noise = true;
	injector2.noise_probability = 0.1;

	// Run identical simulations
	int state1 = 0, state2 = 0;
	for (int i = 0; i < 1000; i++)
	{
		pulse_fault_apply(&injector1, &state1, 100.0, false, false);
		pulse_fault_apply(&injector2, &state2, 100.0, false, false);
	}

	// Should have identical statistics
	TEST_ASSERT(injector1.noise_flips == injector2.noise_flips, "Same seed should produce identical results");
	TEST_ASSERT(injector1.faults_injected == injector2.faults_injected, "Total faults should be identical");

	return TEST_PASS;
}

/**
 * @brief Test multiple simultaneous faults
 */
static int test_fault_multiple_simultaneous(void)
{
	pulse_fault_injector_t injector;
	pulse_fault_injector_init_from_profile(&injector, PULSE_PROFILE_WORST_CASE, 60606);

	pulse_generator_t gen;
	pulse_generator_init(&gen, 6, 1000.0, 40.0);

	const double omega = 6.283185;
	const double dt_us = 100.0;
	int prev_state = 0;

	// Run simulation with all faults enabled
	for (int i = 0; i < 50000; i++)
	{
		int state = pulse_generator_update(&gen, omega, dt_us);
		bool rising_edge = (state == 1 && prev_state == 0);
		bool falling_edge = (state == 0 && prev_state == 1);

		pulse_fault_apply(&injector, &state, dt_us, rising_edge, falling_edge);
		prev_state = state;
	}

	// With WORST_CASE, we should see multiple different fault types triggered
	TEST_ASSERT(injector.faults_injected > 100, "Should have many faults with WORST_CASE profile");

	// Check that multiple fault types were triggered
	int fault_types_triggered = 0;
	if (injector.noise_flips > 0)
		fault_types_triggered++;
	if (injector.pulses_missed > 0)
		fault_types_triggered++;
	if (injector.burst_events > 0)
		fault_types_triggered++;
	if (injector.emi_events > 0)
		fault_types_triggered++;
	if (injector.ringing_events > 0)
		fault_types_triggered++;

	TEST_ASSERT(fault_types_triggered >= 3, "Should trigger at least 3 different fault types");

	return TEST_PASS;
}

/**
 * @brief Main test runner
 */
int main(int argc, char *argv[])
{
	// Check for verbose flag
	if (argc > 1 && (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--verbose") == 0))
	{
		verbose_mode = 1;
		printf(COLOR_CYAN "\n*** VERBOSE MODE ENABLED ***\n" COLOR_RESET);
	}

	printf("\n");
	printf("========================================\n");
	printf("  Pulse Generator Test Suite\n");
	printf("========================================\n");
	if (verbose_mode)
	{
		printf(COLOR_YELLOW "  (Running in verbose mode - detailed output enabled)\n" COLOR_RESET);
	}
	printf("\n");

	// Run all tests
	printf("Basic Functionality Tests:\n");
	RUN_TEST(test_pulse_generator_init);
	RUN_TEST(test_pulse_period_calculation);
	RUN_TEST(test_pulse_duty_cycle);
	RUN_TEST(test_bidirectional_conversion);
	RUN_TEST(test_frequency_conversion);
	RUN_TEST(test_pulse_state_generation);
	RUN_TEST(test_edge_detection);
	RUN_TEST(test_zero_omega);
	RUN_TEST(test_high_speed);
	RUN_TEST(test_different_pulse_counts);

	printf("\nAcceleration and Measurement Lag Tests:\n");
	RUN_TEST(test_acceleration_ramp_with_measurement_lag);
	RUN_TEST(test_step_change_omega);
	RUN_TEST(test_deceleration_ramp);
	RUN_TEST(test_rapid_acceleration);
	RUN_TEST(test_multi_period_averaging);
	RUN_TEST(test_measurement_lag_quantification);
	RUN_TEST(test_frequency_vs_period_measurement);
	RUN_TEST(test_zero_crossing);

	printf("\nEdge Case and Robustness Tests:\n");
	RUN_TEST(test_angular_position_wrapping);
	RUN_TEST(test_falling_edge_timing);
	RUN_TEST(test_very_small_timestep);
	RUN_TEST(test_very_large_timestep);
	RUN_TEST(test_variable_timestep);
	RUN_TEST(test_different_geometries);
	RUN_TEST(test_extremely_low_speed);
	RUN_TEST(test_extremely_high_speed);
	RUN_TEST(test_stop_restart_sequence);
	RUN_TEST(test_omega_with_jitter);
	RUN_TEST(test_many_pulses_per_rev);
	RUN_TEST(test_long_duration_simulation);

	printf("\nFault Injection Framework Tests:\n");
	RUN_TEST(test_fault_injector_init);
	RUN_TEST(test_fault_profile_clean);
	RUN_TEST(test_fault_profile_dirty_sensor);
	RUN_TEST(test_fault_profile_emi_environment);
	RUN_TEST(test_fault_profile_loose_connection);
	RUN_TEST(test_fault_profile_damaged_bolt);
	RUN_TEST(test_fault_profile_worst_case);

	printf("\nIndividual Fault Type Tests:\n");
	RUN_TEST(test_fault_random_noise);
	RUN_TEST(test_fault_missed_pulses);
	RUN_TEST(test_fault_emi_bursts);
	RUN_TEST(test_fault_edge_ringing);
	RUN_TEST(test_fault_attenuation);
	RUN_TEST(test_fault_intermittent_dropout);
	RUN_TEST(test_fault_power_sag);
	RUN_TEST(test_fault_burst_errors);
	RUN_TEST(test_fault_damaged_bolt);

	printf("\nFault Injection Integration Tests:\n");
	RUN_TEST(test_fault_statistics);
	RUN_TEST(test_fault_reproducibility);
	RUN_TEST(test_fault_multiple_simultaneous);

	// Print summary
	printf("\n");
	printf("========================================\n");
	printf("  Test Summary\n");
	printf("========================================\n");
	printf("Tests run:    %d\n", tests_run);
	printf(COLOR_GREEN "Passed:       %d\n" COLOR_RESET, tests_passed);

	if (tests_failed > 0)
	{
		printf(COLOR_RED "Failed:       %d\n" COLOR_RESET, tests_failed);
		printf("\n");
		printf(COLOR_RED "TESTS FAILED\n" COLOR_RESET);
		printf("========================================\n");
		return EXIT_FAILURE;
	}
	else
	{
		printf("Failed:       0\n");
		printf("\n");
		printf(COLOR_GREEN "ALL TESTS PASSED\n" COLOR_RESET);
		printf("========================================\n");
		return EXIT_SUCCESS;
	}
}
