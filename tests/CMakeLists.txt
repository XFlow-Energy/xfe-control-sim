# Test executables for xfe-control-sim

# Macro to create test targets with consistent configuration
macro(add_xfe_test test_name)
    add_executable(${test_name} ${test_name}.c)
    target_link_libraries(${test_name} PRIVATE xfe-control-sim-lib)
    target_include_directories(${test_name} PRIVATE
        ${XFE_CONTROL_SIM_PROJECT_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(UNIX)
        target_link_libraries(${test_name} PRIVATE m)
    endif()
endmacro()

# Test for pulse generator functions
# DISABLED: depends on pulse_generator_* and pulse_fault_* APIs that live in
# in-progress production code not yet on this branch.
# add_xfe_test(test_pulse_generator)

# Numerical integrator tests: each test compiles to its own binary so that
# stateful integrators (AB2 holds f(x_{n-1}) in function-static storage that
# persists for the lifetime of the process) start fresh each invocation.
# Same source file, different RUN_ONLY_TEST define per binary.
set(NUMERICAL_INTEGRATOR_TESTS
    test_euler_integrator_exists
    test_rk4_integrator_exists
    test_ab2_integrator_exists
    test_numerical_integrator_map_has_entries
    test_integrator_map_contains_expected_functions
    test_integrators_handle_zero_timestep
    test_integrators_handle_multiple_state_vars
    test_integrators_handle_single_state_var
    test_integrators_handle_negative_timestep
    test_integrators_repeated_calls
    test_euler_vs_rk4_different_results
    test_euler_accuracy_decay
    test_rk4_accuracy_decay
    test_ab2_accuracy_decay
    test_integrator_accuracy_ordering
    test_euler_convergence_order
)
foreach(test_fn IN LISTS NUMERICAL_INTEGRATOR_TESTS)
    add_executable(${test_fn} test_numerical_integrator.c)
    target_link_libraries(${test_fn} PRIVATE xfe-control-sim-lib)
    target_include_directories(${test_fn} PRIVATE
        ${XFE_CONTROL_SIM_PROJECT_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_compile_definitions(${test_fn} PRIVATE RUN_ONLY_TEST=${test_fn})
    if(UNIX)
        target_link_libraries(${test_fn} PRIVATE m)
    endif()
endforeach()

# Test for flow generation and interpolation
add_xfe_test(test_flow_gen)

# Test for parameter array system
add_xfe_test(test_param_array)

# Test for common utility functions
add_xfe_test(test_common_utils)

# Test for control switch and stage dispatch
add_xfe_test(test_control_switch)
