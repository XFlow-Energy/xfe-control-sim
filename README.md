![architecture](https://docs.google.com/drawings/d/1p16TDVVJEGd-Ujmj3uxu26vpOR3q9V4ihnMTAvmehr0/export/png)

# Aero-Control Project Documentation

This project is structured around xfe-control-sim systems, utilizing various modules for aerodynamic models, control systems, and integrations with the XFlow wind turbine simulation. The build system is managed using CMake, and the project integrates QBlade for testing and validation.

## Getting Started

### Installing Dependencies
To install dependencies, read the [README.md](misc/README.md) file located in `misc`.

### Running Example Program to Test Installation on Unix systems
Run the `launch_xfe_control_sim_example.sh` script located in `misc`:
```bash
./misc/launch_xfe_control_sim_example.sh
```

### Configuring Your Own Project
To start modifying your own project, copy `sim_example` to the same folder location as `xfe-control-sim` and use that as your main project location. To compile within that project, run:
```bash
./misc/launch_xfe_control_sim.sh
```
or on windows power shell:
```bash
./misc/run_xfe_control_sim.ps1
```

Or compile using the Visual Studio Code CMake integration.

## Table of Contents

- [Project Overview](#project-overview)
- [Directory Structure](#directory-structure)
  - [cmake](#cmake)
  - [include](#include)
  - [log](#log)
  - [matlab_processing](#matlab_processing)
  - [misc](#misc)
  - [QBlade](#qblade)
  - [sim_example](#sim_example)
  - [src](#src)
  - [CMakeLists.txt](#cmakelists.txt)
  - [iwyu_mappings.imp](#iwyu_mappingsimp)
  - [LICENSE](#license)
  - [README.md](#readmemd)
- [Data Management](#data-management)
  - [Data Structures](#data-structures)
  - [Functions](#functions)
    - [Adding Parameters](#adding-parameters)
    - [Retrieving Parameters](#retrieving-parameters)
    - [Data Initialization](#data-initialization)
    - [Data Logging](#data-logging)
      - [Header Logging](#header-logging)
      - [Processed Data Logging](#processed-data-logging)
  - [Dynamic vs Fixed Data](#dynamic-vs-fixed-data)
  - [Example Usage](#example-usage)
- [Build Functions](#build-functions)
  - [MAKE_STAGE and MAKE_STAGE_DEFINE](#make_stage-and-make_stage_define)
    - [Example: `numerical_integrator`](#example-numerical_integrator)
  - [Map Arrays and Dispatching](#map-arrays-and-dispatching)
  - [Function-Specific Details](#function-specific-details)
    - [`numerical_integrator`](#numerical_integrator)
    - [`flow_gen`](#flow_gen)
    - [`flow_sim_model`](#flow_sim_model)
    - [`drivetrain`](#drivetrain)
    - [`eom` (Equation of Motion)](#eom-equation-of-motion)
    - [`turbine_control`](#turbine_control)
    - [`qblade_interface`](#qblade_interface)
    - [`data_processing`](#data_processing)
  - [Summary of Macro-Based Workflow](#summary-of-macro-based-workflow)
- [Functions Specified at Build (using Configuration CSV)](#functions-specified-at-build-using-configuration-csv)
- [Conditional Compilation for Libraries and Executables](#conditional-compilation-for-libraries-and-executables)
- [To compile using Linux](#to-compile-using-linux)
- [Adding New Devices (e.g., Drivetrains, Flow Generators, etc.)](#adding-new-devices-eg-drivetrains-flow-generators-etc)
  - [1. Implement Your New Function in the Consolidated File](#1-implement-your-new-function-in-the-consolidated-file)
  - [2. Create or Update the Configuration CSV](#2-create-or-update-the-configuration-csv)
  - [3. No CMake Changes Needed for New Functions](#3-no-cmake-changes-needed-for-new-functions)
  - [4. Updating Feature Flags](#4-updating-feature-flags)
  - [5. Verification Steps](#5-verification-steps)
- [Additional Information](#additional-information)
- [CSV Structure](#csv-structure)
- [Using and Adapting `sim_example` for Customer Simulations](#using-and-adapting-sim_example-for-customer-simulations)
  - [1. Directory Placement](#1-directory-placement)
  - [2. Cloning or Copying `sim_example`](#2-cloning-or-copying-sim_example)
  - [3. Customizing the CMake Logic](#3-customizing-the-cmake-logic)
  - [4. Adapting Sim Files](#4-adapting-sim-files)
  - [5. Running the Example or Customer Simulation](#5-running-the-example-or-customer-simulation)
  - [6. Version Control Strategy for Customers](#6-version-control-strategy-for-customers)
- [Summary](#summary)

---

## Project Overview

The xfe-control-sim project is designed to simulate and control aerodynamic models for wind turbines. It is built using CMake and integrates various modules such as drivetrain models, aerodynamic models, and control systems. The project also uses QBlade for testing aerodynamic performance under different configurations.

## Directory Structure

### cmake

- **cmake/default_control_wildcard.cmake**: Provides wildcard handling for control configurations.  
- **cmake/dependencies.cmake**: Manages external dependencies required by the project.  
- **cmake/detect_ccache.cmake**: Detects and configures ccache to speed up compiles.  
- **cmake/ipo.cmake**: Sets up interprocedural optimization flags.  
- **cmake/modules/Findjansson.cmake**: CMake module to locate Jansson (JSON) library.  
- **cmake/modules/Findlibmodbus.cmake**: CMake module to locate libmodbus library.  

### include

Contains header files defining interfaces and shared functionality across the project:  

- **include/xfe_control_sim_common.h**: Common definitions and utility functions for aerodynamic components.  
- **include/xfe_control_sim_version.in.h**: Template for versioning information used by CMake.  
- **include/bladed_interface.h**: Interface definitions for communicating with Bladed/QBlade.  
- **include/control_switch.h**: Definitions for control switch mechanisms.  
- **include/flow_gen.h**: Interfaces for wind data generation and interpolation.  
- **include/make_stage.h**: Definitions related to build stages and code organization.  
- **include/numerical_integrator.h**: Interfaces for numerical integrators (e.g., Runge-Kutta).  

### log

Stores log and output data generated during development and simulations:  

- **log/*.csv**: Various CSV log files (e.g., `data_processing_data_export*.csv`, `bts_velocity_*.csv`).  
- **log/log_data/xfe-control-sim-simulation-output.log**: Core simulation output log.  
- **log/README.md**: Instructions and descriptions for log files.  

### matlab_processing

Contains MATLAB scripts and data files for post-processing and analysis:  

- **matlab_processing/aep_calc_data.mat**, **data_eff.mat**, **powertrain_eff.mat**, etc.: MATLAB data files used in analysis.  
- **matlab_processing/blended_cp_curve.m**, **eval_efficiency.m**, **generate_ramp_time_series.m**, etc.: MATLAB scripts for data visualization and efficiency calculations.  
- **matlab_processing/three_d_color_plot.m**, **two_d_color_plot.m**: Scripts to generate color plots of simulation data.  

### misc

Miscellaneous scripts and utilities for building, installation, and launching examples:  

- **misc/add_header.py**: Script to add standard headers to source files.  
- **misc/clang_format_all.sh**: Formats code using clang-format.  
- **misc/gui.py**, **misc/plot_viewer.py**: Python scripts for launching GUI tools.  
- **misc/*.ps1**: PowerShell scripts to install dependencies (e.g., Clang, CMake, GSL, Jansson, libmodbus, LLVM Mingw, Ninja).  
- **misc/*.sh**: Bash scripts to launch simulations and GUIs (e.g., `launch_xfe_control_sim_example.sh`, `launch_sim_example_test.sh`).  
- **misc/install_unix_dependencies.sh**: Bash script to install dependencies for unix machines.  
- **misc/launch_install_plot_gui.sh**: Bash script to install and run the gui interface

### QBlade

- **QBlade/XFlow_controller_integration_test.qpr**: A QBlade project file used for integration testing of aerodynamic performance.  

### sim_example

A self-contained example demonstrating how to build and run a simulation:  

- **sim_example/cmake/dependencies.cmake**: Dependency management for the simulation example.  
- **sim_example/CMakeLists.txt**: Build configuration for the simulation example.  
- **sim_example/config/flow/**: Contains wind data files (`demo_wind_file*.bts`, `turb_train_data*csv`) used by the example.  
- **sim_example/config/simple_ball_config.cmake**: Example configuration for a simple simulation.  
- **sim_example/config/simple_ball_config.csv**: Example CSV configuration file.  
- **sim_example/include/**: Header files specific to the simulation example:  
  - **sim_example/include/flow_sim_model.h**  
  - **sim_example/include/data_processing.h**  
  - **sim_example/include/drivetrains.h**  
  - **sim_example/include/equation_of_motion.h**  
  - **sim_example/include/qblade_interface.h**  
  - **sim_example/include/turbine_controls.h**  
- **sim_example/log/README.md**: Instructions for example log files.  
- **sim_example/src/**: Source code for the simulation example:  
  - **sim_example/src/flow_sim_model.c**  
  - **sim_example/src/data_processing.c**  
  - **sim_example/src/drivetrain.c**  
  - **sim_example/src/equation_of_motion.c**  
  - **sim_example/src/qblade_interface/discon.c**, **discon.in**, **qblade_control_switch.c**, **qblade_interface.c**, **test_discon.c**  
  - **sim_example/src/turbine_control.c**  

### src

Contains the primary source code for the xfe-control-sim library and applications:  

- **src/CMakeLists.txt**: Build configuration for the main project.  
- **src/xfe_control_sim_common.c**: Implements shared functions used by multiple modules.  
- **src/xfe_control_sim_main.c**: Entry point (`main()`) and top-level control logic.  
- **src/control_switch.c**: Implements control switch logic referenced in simulations.  
- **src/flow_gen.c**: Implements wind data input and interpolation routines.  
- **src/modbus_server.c**: Modbus TCP server implementation for remote control.  
- **src/numerical_integrator.c**: Implements numerical integration routines (e.g., Runge-Kutta).  
- **src/turbine_control_common.c**: Shared turbine control functions.  

### CMakeLists.txt

The top-level CMake configuration for building the entire project, including subdirectories and external dependencies.  

### iwyu_mappings.imp

Include-What-You-Use mappings file to assist with dependency tracking and header organization.  

### LICENSE

This project is licensed under the GNU General Public License v3 (GPLv3).
See the [LICENSE](LICENSE) file for the full text of the license.

#### Exception: `sim_example` Directory

The files located in the `/sim_example` directory are an exception to the project's GPLv3 license. All contents of this directory are dedicated to the public domain under the **CC0 1.0 Universal** license, allowing you to copy, modify, and use them in your own projects without any restrictions. See the `sim_example/LICENSE` file for details.

### README.md

This README file providing an overview and instructions for the project.

## Data Management

In the xfe-control-sim system, dynamic and fixed data are managed using a pointer-based structure, where both types of data are stored in `param_array_t` structures. These structures are passed by reference to functions, allowing for flexible memory allocation and efficient data manipulation.

The dynamic and fixed data are initialized by functions that populate them with parameters read from configuration files. Parameters are added dynamically to these structures, with each parameter stored as an entry containing its name, type, and value.

To retrieve parameters, the system uses functions that return pointers to the specific values (which can be integers, doubles, or strings) by name. This approach ensures that we are working with the original data in the `param_array_t` structure without duplicating values.

The flexibility of this system allows for easy modification and retrieval of parameters during simulations, promoting efficient data handling. Functions like `add_param` dynamically add or update parameters, while `get_param` retrieves the correct value, making data access modular and effective throughout the simulation process.

When accessing data using `get_param`, we receive a pointer to the parameter’s value directly within the `param_array_t` structure. This ensures efficient memory usage and allows real-time updates, as any changes made via the pointer are reflected directly in the original data structure.

### Data Structures

#### `input_param_t`

Stores individual parameters with the following fields:
- **name**: Name of the parameter.
- **type**: Type of the parameter (int, double, or string).
- **is_state_var**: `true` if this parameter represents a state variable.
- **value**: A union storing the actual value based on the parameter type.

```c
typedef struct {
    char *name;
    input_param_type_t type;
    bool is_state_var;       // True if part of the state vector
    union {
        int i;
        double d;
        char *s;
    } value;
} input_param_t;
```

#### `param_array_t`

Represents an array of `input_param_t` parameters. It keeps track of the number of parameters and provides easy access to them by name.

```c
typedef struct {
    int n_param;             // Number of parameters
    input_param_t *params;   // Array of parameters
} param_array_t;
```

## Functions

#### Adding Parameters

Parameters are added or updated using the `add_param` function. If a parameter already exists (identified by name), it will be updated; otherwise, it will be added.

```c
void add_param(param_array_t *data, const char *name, input_param_type_t type, void *value);
```

#### Retrieving Parameters

Parameters can be retrieved using the `get_param` function, which assigns the correct value based on the type (int, double, or string).

```c
void get_param(param_array_t *param_array, const char *name, void *out_ptr);
```

#### Data Initialization

The `initialize_control_system` function sets up the dynamic and fixed data structures and loads data from CSV configuration files.

```c
void initialize_control_system(param_array_t **dynamic_data, param_array_t **fixed_data);
```

#### Data Logging

The system can save dynamic and fixed data to CSV files for logging purposes during single runs. The `save_param_array_data_to_csv` function writes data to a CSV file, including a header if desired.

```c
void save_param_array_data_to_csv(const char *filename, param_array_t *data, int write_header);
```

When enabling data processing, these two functions below are used for data logging:

##### Header Logging

The `save_csv_header` function writes a header row to a CSV file. This is typically called once before data collection begins. It writes an "epoch_time" column followed by user-defined column headers. A semaphore is used to ensure exclusive access to the file.

```c
void save_csv_header(const char *filename, semaphore_info_t *sem_info, const char **headers);
```

##### Processed Data Logging

The `save_double_array_data_to_csv` function appends a row of data to the CSV file. The first column is a high-precision epoch timestamp, and subsequent columns are the double values corresponding to the provided headers. A semaphore ensures thread-safe file access.

```c
void save_double_array_data_to_csv(const char *filename, semaphore_info_t *sem_info, const double *data, int n_data);
```

These two functions are typically used in the data processing logic after the single values are calculated from each run. First, `save_csv_header` is called once to initialize the CSV file, and then `save_double_array_data_to_csv` is called iteratively as new data is processed.

This separation allows for more flexible and efficient logging behavior, especially when operating in environments where multiple threads or processes may be involved.

### Dynamic vs Fixed Data

- **Dynamic Data**: Represents values that change over time during the simulation, like `time_sec`.  
- **Fixed Data**: Represents constants or parameters that do not change during the simulation, such as `dt_sec` (time step) and `dur_sec` (duration).  

In the main simulation loop, dynamic parameters are updated continuously, while fixed parameters remain constant.

### Example Usage

- **Initialization**:

```c
initialize_control_system(&dynamic_data, &fixed_data);
```

- **Retrieving Parameters**:

```c
static double *dt_Sec = NULL;
static double *dur_Sec = NULL;
static double *time_Sec = NULL;

get_param(fixed_data, "dt_sec", &dt_Sec);
get_param(fixed_data, "dur_sec", &dur_Sec);
get_param(dynamic_data, "time_sec", &time_Sec);
```

- **Adding a Parameter**:

```c
double new_value = 3.14;
add_param(dynamic_data, "some_param", INPUT_PARAM_DOUBLE, &new_value);
```

This flexible data structure allows efficient handling of both dynamic and fixed simulation parameters, providing modularity and ease of maintenance.

## Build Functions

The build functions in this project are implemented using a macro-based stage registration mechanism to allow modular selection and dynamic dispatch at runtime. Instead of manually wiring each function call, the following macros and conventions are used:

### MAKE_STAGE and MAKE_STAGE_DEFINE

Two primary macros control the build function generation:

1. **`MAKE_STAGE(name, RTYPE, PARAMS)`**  
   Declares:  
   - A function pointer type: `typedef RTYPE (*name##_fn) PARAMS;`  
   - A registration function: `void register_##name(name##_fn fn);`  
   - A dispatcher function: `RTYPE name PARAMS;`  
   - A map entry struct:  
     ```c
     typedef struct {
         const char *id;
         name##_fn fn;
     } name##Map;
     ```  
   This sets up the stage to accept callbacks and exposes a default `name()` dispatch function.

2. **`MAKE_STAGE_DEFINE(name, RTYPE, PARAMS, ARGS)`**  
   Used in exactly one `.c` file to:  
   - Define a static callback pointer `static name##_fn name##_cb = NULL;`.  
   - Implement `register_##name(name##_fn fn)` to assign to `name##_cb`.  
   - Provide a `default_##name` implementation that logs an error and sets `shutdownFlag`.  
   - Register the default callback via a constructor function (`__attribute__((constructor))`).  
   - Define the dispatcher `RTYPE name PARAMS` that:  
     - Calls `name##_cb ARGS` if set.  
     - Otherwise logs an error and sets `shutdownFlag`.

   The `PARAMS` macro parameter is the full function signature list `(type1 arg1, type2 arg2, ...)`, and `ARGS` is the list of argument names `(arg1, arg2, ...)`.

#### Example: `numerical_integrator`

In `numerical_integrator.h`:
```c
#define NUMERICAL_INTEGRATOR_PARAM_LIST     MAYBE_UNUSED double **state_vars,     MAYBE_UNUSED const char **state_names,     MAYBE_UNUSED const int n_state_var,     MAYBE_UNUSED const double dt,     MAYBE_UNUSED const param_array_t *dynamic_data,     MAYBE_UNUSED const param_array_t *fixed_data

#define NUMERICAL_INTEGRATOR_CALL_ARGS     state_vars, state_names, n_state_var, dt, dynamic_data, fixed_data

MAKE_STAGE(numerical_integrator, void, (NUMERICAL_INTEGRATOR_PARAM_LIST));

void ab2_numerical_integrator(NUMERICAL_INTEGRATOR_PARAM_LIST);
void euler_numerical_integrator(NUMERICAL_INTEGRATOR_PARAM_LIST);
void rk4_numerical_integrator(NUMERICAL_INTEGRATOR_PARAM_LIST);

static const numerical_integratorMap numerical_integrator_map[] = {
    {"ab2_numerical_integrator",   ab2_numerical_integrator},
    {"euler_numerical_integrator", euler_numerical_integrator},
    {"rk4_numerical_integrator",   rk4_numerical_integrator},
};
```

In `numerical_integrator.c`:
```c
MAKE_STAGE_DEFINE(
    numerical_integrator,
    void,
    (NUMERICAL_INTEGRATOR_PARAM_LIST),
    (NUMERICAL_INTEGRATOR_CALL_ARGS)
);
```

### Map Arrays and Dispatching

Each build function has a corresponding static array in its header file that maps string identifiers to actual implementations. For example:
```c
static const numerical_integratorMap numerical_integrator_map[] = {
    {"ab2_numerical_integrator",   ab2_numerical_integrator},
    {"euler_numerical_integrator", euler_numerical_integrator},
    {"rk4_numerical_integrator",   rk4_numerical_integrator},
};
```
- The key is the string (e.g., `"rk4_numerical_integrator"`).  
- The value is the function pointer (e.g., `rk4_numerical_integrator`).

To select an implementation at runtime:
```c
DISPATCH_STAGE_OR_ERROR(
    numerical_integrator,
    numerical_integrator_map,
    chosen_identifier
);
```
This invokes `dispatch_numerical_integrator(chosen_identifier)`, which:
1. Iterates through `numerical_integrator_map`.  
2. If the identifier matches `map[i].id`, calls `register_numerical_integrator(map[i].fn)` and returns `true`.  
3. If no match is found, logs an error, prints valid options, and sets `shutdownFlag`.

Once registered, calls to:
```c
numerical_integrator(state_vars, state_names, n_state_var, dt, dynamic_data, fixed_data);
```
will dispatch to the chosen implementation.

### Function-Specific Details

Below are the main build functions and their macro-based setup. Each uses the same pattern: a `MAKE_STAGE` declaration, a static map array, a `MAKE_STAGE_DEFINE` in one `.c`, and a `DISPATCH_STAGE_OR_ERROR` call before use.

#### `numerical_integrator`
- **Header (`numerical_integrator.h`)**:  
  Contains `MAKE_STAGE(numerical_integrator, ...)`, the prototypes for `ab2_numerical_integrator`, `euler_numerical_integrator`, `rk4_numerical_integrator`, and `numerical_integrator_map[]`.
- **Source (`numerical_integrator.c`)**:  
  Uses `MAKE_STAGE_DEFINE` to generate the dispatcher, default, and registration.
- **Runtime**:
```c
DISPATCH_STAGE_OR_ERROR(numerical_integrator, numerical_integrator_map, "rk4_numerical_integrator");
numerical_integrator(state_vars, state_names, n_state_var, dt, dynamic_data, fixed_data);
```

#### `flow_gen`
- **Header (`flow_gen.h`)**:
```c
#define FLOW_GEN_PARAM_LIST       MAYBE_UNUSED const param_array_t *dynamic_data,       MAYBE_UNUSED const param_array_t *fixed_data

#define FLOW_GEN_CALL_ARGS       dynamic_data, fixed_data

MAKE_STAGE(flow_gen, void, (FLOW_GEN_PARAM_LIST));

void bts_fixed_interp_flow_gen(FLOW_GEN_PARAM_LIST);
void csv_fixed_interp_flow_gen(FLOW_GEN_PARAM_LIST);

static const flow_genMap flow_map[] = {
    {"csv_fixed_interp_flow_gen", csv_fixed_interp_flow_gen},
    {"bts_fixed_interp_flow_gen", bts_fixed_interp_flow_gen},
};
```
- **Source (`flow_gen.c`)**:
```c
MAKE_STAGE_DEFINE(flow_gen, void, (FLOW_GEN_PARAM_LIST), (FLOW_GEN_CALL_ARGS));

__attribute__((constructor)) static void init_flow_gen_hook(void)
{
    register_flow_gen(csv_fixed_interp_flow_gen);
}
```
The constructor registers `csv_fixed_interp_flow_gen` as the default implementation.

- **Runtime**:
```c
DISPATCH_STAGE_OR_ERROR(flow_gen, flow_map, "bts_fixed_interp_flow_gen");
flow_gen(dynamic_data, fixed_data);
```

#### `flow_sim_model`
- **Header (`flow_sim_model.h`)**:
```c
#define FLOW_SIM_MODEL_PARAM_LIST       MAYBE_UNUSED const param_array_t *dynamic_data,       MAYBE_UNUSED const param_array_t *fixed_data

#define FLOW_SIM_MODEL_CALL_ARGS       dynamic_data, fixed_data

MAKE_STAGE(flow_sim_model, void, (FLOW_SIM_MODEL_PARAM_LIST));

void example_flow_sim_model(FLOW_SIM_MODEL_PARAM_LIST);

static const flow_sim_modelMap flow_sim_model_map[] = {
    {"example_flow_sim_model", example_flow_sim_model},
};
```
- **Source (`flow_sim_model.c` or similar)**:
```c
MAKE_STAGE_DEFINE(flow_sim_model, void, (FLOW_SIM_MODEL_PARAM_LIST), (FLOW_SIM_MODEL_CALL_ARGS));
__attribute__((constructor)) static void init_flow_sim_model_hook(void)
{
    register_flow_sim_model(example_flow_sim_model);
}
```
- **Runtime**:
```c
DISPATCH_STAGE_OR_ERROR(flow_sim_model, flow_sim_model_map, "example_flow_sim_model");
flow_sim_model(dynamic_data, fixed_data);
```

#### `drivetrain`
- **Header (`drivetrains.h`)**:
```c
#define DRIVETRAIN_PARAM_LIST       MAYBE_UNUSED const param_array_t *dynamic_data,       MAYBE_UNUSED const param_array_t *fixed_data

#define DRIVETRAIN_CALL_ARGS       dynamic_data, fixed_data

MAKE_STAGE(drivetrain, void, (DRIVETRAIN_PARAM_LIST));

void example_drivetrain(DRIVETRAIN_PARAM_LIST);

static const drivetrainMap drivetrain_map[] = {
    {"example_drivetrain", example_drivetrain},
};
```
- **Source (`drivetrain.c` or similar)**:
```c
MAKE_STAGE_DEFINE(drivetrain, void, (DRIVETRAIN_PARAM_LIST), (DRIVETRAIN_CALL_ARGS));
__attribute__((constructor)) static void init_drivetrain_hook(void)
{
    register_drivetrain(example_drivetrain);
}
```
- **Runtime**:
```c
DISPATCH_STAGE_OR_ERROR(drivetrain, drivetrain_map, "example_drivetrain");
drivetrain(dynamic_data, fixed_data);
```

#### `eom` (Equation of Motion)
- **Header (`equation_of_motion.h`)**:
```c
#define EOM_PARAM_LIST       MAYBE_UNUSED double **state_vars,       MAYBE_UNUSED const char **state_names,       MAYBE_UNUSED const int n_state_var,       MAYBE_UNUSED double *dx,       MAYBE_UNUSED const param_array_t *dynamic_data,       MAYBE_UNUSED const param_array_t *fixed_data

#define EOM_CALL_ARGS       state_vars, state_names, n_state_var, dx, dynamic_data, fixed_data

MAKE_STAGE(eom, void, (EOM_PARAM_LIST));

void eom_simple_ball_thrown_in_air(EOM_PARAM_LIST);

static const eomMap eom_map[] = {
    {"eom_simple_ball_thrown_in_air", eom_simple_ball_thrown_in_air},
};
```
- **Source (`equation_of_motion.c` or similar)**:
```c
MAKE_STAGE_DEFINE(eom, void, (EOM_PARAM_LIST), (EOM_CALL_ARGS));
__attribute__((constructor)) static void init_eom_hook(void)
{
    register_eom(eom_simple_ball_thrown_in_air);
}
```
- **Runtime**:
```c
DISPATCH_STAGE_OR_ERROR(eom, eom_map, "eom_simple_ball_thrown_in_air");
eom(state_vars, state_names, n_state_var, dx, dynamic_data, fixed_data);
```

#### `turbine_control`
- **Header (`turbine_controls.h` in `sim_example/include`)**:
```c
#define TURBINE_CONTROL_PARAM_LIST       MAYBE_UNUSED const param_array_t *dynamic_data,       MAYBE_UNUSED const param_array_t *fixed_data

#define TURBINE_CONTROL_CALL_ARGS       dynamic_data, fixed_data

MAKE_STAGE(turbine_control, void, (TURBINE_CONTROL_PARAM_LIST));

void example_turbine_control(TURBINE_CONTROL_PARAM_LIST);
void kw2_turbine_control(TURBINE_CONTROL_PARAM_LIST);

static const turbine_controlMap turbine_control_map[] = {
    {"example_turbine_control", example_turbine_control},
    {"kw2_turbine_control",     kw2_turbine_control    },
};
```
- **Source (`turbine_control.c` in `sim_example/src`)**:
```c
MAKE_STAGE_DEFINE(turbine_control, void, (TURBINE_CONTROL_PARAM_LIST), (TURBINE_CONTROL_CALL_ARGS));
__attribute__((constructor)) static void init_turbine_control_hook(void)
{
    register_turbine_control(example_turbine_control);
}
```
- **Runtime**:
```c
DISPATCH_STAGE_OR_ERROR(turbine_control, turbine_control_map, "kw2_turbine_control");
turbine_control(dynamic_data, fixed_data);
```

#### `qblade_interface`
- **Header (`qblade_interface.h` in `sim_example/include`)**:
```c
#define QBLADE_INTERFACE_PARAM_LIST       MAYBE_UNUSED float *avr_swap,       MAYBE_UNUSED const param_array_t *dynamic_data,       MAYBE_UNUSED const param_array_t *fixed_data

#define QBLADE_INTERFACE_CALL_ARGS       avr_swap, dynamic_data, fixed_data

MAKE_STAGE(qblade_interface, void, (QBLADE_INTERFACE_PARAM_LIST));

void example_qblade_interface(QBLADE_INTERFACE_PARAM_LIST);

static const qblade_interfaceMap qblade_interface_map[] = {
    {"example_qblade_interface", example_qblade_interface},
};
```
- **Source (`qblade_interface.c` in `sim_example/src/qblade_interface`)**:
```c
MAKE_STAGE_DEFINE(qblade_interface, void, (QBLADE_INTERFACE_PARAM_LIST), (QBLADE_INTERFACE_CALL_ARGS));
__attribute__((constructor)) static void init_qblade_interface_hook(void)
{
    register_qblade_interface(example_qblade_interface);
}
```
- **Runtime**:
```c
DISPATCH_STAGE_OR_ERROR(qblade_interface, qblade_interface_map, "example_qblade_interface");
qblade_interface(avr_swap, dynamic_data, fixed_data);
```

#### `data_processing`
- **Header (`data_processing.h` in `sim_example/include`)**:
```c
#define DATA_PROCESSING_PARAM_LIST       MAYBE_UNUSED const param_array_t *dynamic_data,       MAYBE_UNUSED const param_array_t *fixed_data,       MAYBE_UNUSED data_processing_program_args_t *dp_program_options

#define DATA_PROCESSING_CALL_ARGS       dynamic_data, fixed_data, dp_program_options

MAKE_STAGE(data_processing, void, (DATA_PROCESSING_PARAM_LIST));

void example_data_processing(DATA_PROCESSING_PARAM_LIST);

static const data_processingMap data_processing_map[] = {
    {"example_data_processing", example_data_processing},
};
```
- **Source (`data_processing.c` in `sim_example/src` or `src`)**:
```c
MAKE_STAGE_DEFINE(data_processing, void, (DATA_PROCESSING_PARAM_LIST), (DATA_PROCESSING_CALL_ARGS));
__attribute__((constructor)) static void init_data_processing_hook(void)
{
    register_data_processing(example_data_processing);
}
```
- **Runtime**:
```c
DISPATCH_STAGE_OR_ERROR(data_processing, data_processing_map, "example_data_processing");
data_processing(dynamic_data, fixed_data, dp_program_options);
```

### Summary of Macro-Based Workflow

1. **Compile-Time Declarations:**  
   Headers declare stages via `MAKE_STAGE(...)` and list implementations in a `Map[]`.
2. **Compile-Time Definitions:**  
   One `.c` file uses `MAKE_STAGE_DEFINE(...)` to set up dispatcher, default, and registration.
3. **Runtime Selection:**  
   - Use `DISPATCH_STAGE_OR_ERROR(...)` with the map and chosen string to register the callback.  
   - Call the stage as a normal function; it dispatches internally to the chosen callback.
4. **Default Behavior:**  
   If no valid callback is registered (missing or incorrect dispatch), the default implementation logs an error and sets `shutdownFlag`, halting execution.

This mechanism allows clean separation of interface, implementations, and selection logic. Adding new build functions simply requires:
1. Adding `MAKE_STAGE(...)` to the header.  
2. Providing new implementations and adding them to the corresponding `Map[]`.  
3. Ensuring exactly one `MAKE_STAGE_DEFINE(...)` exists for that stage.  
4. Calling `DISPATCH_STAGE_OR_ERROR(...)` at startup or configuration to set the active function.

---

## Functions Specified at Build (using Configuration CSV)

In this xfe-control-sim system, a single configuration CSV file (`system_config.csv`) lists which function to call for each stage. This design decouples CMake configuration from the actual function selection and allows changing behavior by editing the CSV—without a recompilation.

### How the Configuration CSV Drives Function Selection

- At build time, CMake still defines compile‑time constants such as:
  - Paths to directories containing each function’s source (`XFE_CONTROL_SIM_CONFIG_DIR`, `FLOW_GEN_FILE_DIR`, etc.).
  - The filename of the system configuration CSV (`SYSTEM_CONFIG_FULL_PATH`).
  - Logging flags and output paths.

‑ However, CMake no longer needs to know which exact function implementation to link in. Instead, each candidate function’s source file is compiled and linked into the library/executable. The CSV then tells the runtime which one to use.

- **CSV Structure**:  
  The first rows of `system_config.csv` list function names for each stage. For example:
  ```csv
  flow_function_call,char,fixed,csv_fixed_interp_flow_gen
  numerical_integrator_function_call,char,fixed,rk4_numerical_integrator
  turbine_control_function_call,char,fixed,kw2_turbine_control
  eom_function_call,char,fixed,eom_simple_ball_thrown_in_air
  drivetrain_function_call,char,fixed,example_drivetrain
  flow_sim_model_function_call,char,fixed,example_flow_sim_model
  data_processing_function_call,char,fixed,example_data_processing
  qblade_interface_function_call,char,fixed,example_qblade_interface
  ```
  Each of these key–value entries tells the program which implementation (by name) to register at startup.

- **Reasoning**:  
  1. **Flexibility**: Developers or end‑users can switch e.g. from `ab2_numerical_integrator` to `rk4_numerical_integrator` simply by editing the CSV.  
  2. **No Rebuild Needed**: Since all implementations are already compiled, changing a CSV entry immediately takes effect the next time the simulation runs.  
  3. **Simplified CMake**: Build scripts only need to compile the full set of available implementations; they do not need to conditionally enable or disable individual files based on user choices.

### `control_switch`: Reading from the CSV and Dispatching

The central logic tying the CSV to actual function calls lives in `control_switch.c`. Its job is to:

1. **Load the CSV at Runtime**  
   - The CSV file is read into a `param_array_t` structure before `main()` begins its main loop.  
   - CMake ensures the compile definition `SYSTEM_CONFIG_FULL_PATH` points to the correct CSV.

2. **Retrieve Function‑Name Strings**  
   - On first execution of `control_switch()`, it calls:
     ```c
     get_param(fixed_data, "flow_function_call", &flow_Function_Call);
     get_param(fixed_data, "numerical_integrator_function_call", &numerical_Integrator_Function_Call);
     // ... and similarly for other stages
     ```
   - Each `*_function_call` variable is a string containing the chosen callback’s identifier (e.g. `"rk4_numerical_integrator"`).

3. **Register Callbacks via Stage Dispatchers**  
   - For every stage, `control_switch` invokes:
     ```c
     DISPATCH_STAGE_OR_ERROR(stage_name, stage_map, chosen_identifier);
     ```
   - Under the hood, each `DEFINE_STAGE_DISPATCHER(name, name##_map)` macro created a helper `dispatch_name()` that:
     - Iterates through `name##_map` (an array of `{ "id", fn }` pairs declared in the corresponding header).  
     - If `id` matches the string from the CSV, it calls `register_name(fn)` to store that function pointer.  
     - If no match is found, logs an error listing valid IDs and sets `shutdownFlag` to abort.

4. **After Dispatch, Invoke as Normal Functions**  
   - Once `register_*()` has been called, any subsequent call to `stage_name(...)` invokes the chosen implementation.  
   - Example:
     ```c
     // In main simulation loop:
     flow_gen(dynamic_data, fixed_data);
     numerical_integrator(state_vars, state_names, n_state_var, dt, dynamic_data, fixed_data);
     turbine_control(dynamic_data, fixed_data);
     // etc.
     ```
   - If `register_stage` was never called (or failed), the default implementation (injected by `MAKE_STAGE_DEFINE`) logs an error and shuts down.

### Benefits and Rationale

- **Separation of Concerns**  
  - **CMake**: Only responsible for locating source directories (via variables like `FLOW_GEN_DIR`) and ensuring every candidate implementation is compiled.  
  - **CSV Config**: Determines at runtime which implementation to use, without altering compiled binaries.

- **Extensibility**  
  - Adding a new function (e.g., `my_custom_flow_gen`) only requires:
    1. Implementing the function in a `.c` file.  
    2. Appending its `{ "my_custom_flow_gen", my_custom_flow_gen }` entry to `flow_map[]` in `flow_gen.h`.  
    3. Updating `system_config.csv` to set `flow_function_call=my_custom_flow_gen`.  
    No changes to CMakeLists.txt or recompilation scripts are needed—only recompilation of the new source.

- **Runtime Safety**  
  - If the CSV contains an invalid string, `DISPATCH_STAGE_OR_ERROR` immediately logs a clear error and stops execution before simulation begins.  
  - This prevents silent misconfiguration and makes it obvious which IDs are valid.

### Revised CMake Role

Even though the CSV handles which functions to call, CMake still:

1. **Defines Paths** to directories where CSV files and source code live:  
   - `set(XFE_CONTROL_SIM_CONFIG_DIR ...)`  
   - `set(FLOW_GEN_FILE_DIR ...)`
2. **Injects Compile‑Time Definitions** for:  
   - `SYSTEM_CONFIG_FULL_PATH` → path to `system_config.csv`  
   - Logging flags (e.g., `LOGGING_DYNAMIC_DATA_CONTINUOUS`, etc.)
3. **Ensures the CSV Is Copied** into the build or runtime directory so that `get_param()` can locate it via `SYSTEM_CONFIG_FULL_PATH`.

### Summary

By migrating from CMake‑only selection to a hybrid approach—compile all implementations, then choose at runtime via a CSV—the system gains:

- **Simplicity**: No need to edit `CMakeLists.txt` every time a new stage implementation is added.  
- **Configurability**: Easy swap of algorithms (e.g., switching integrators) without rebuild.  
- **Clarity**: A single source of truth (`system_config.csv`) lists exactly which functions drive the simulation.

Below is a concise example of how CSV entries, control_switch, and dispatch macros collaborate to instantiate each stage:

1. **CSV Fragment** (`system_config.csv`):
   ```csv
   flow_function_call,char,fixed,csv_fixed_interp_flow_gen
   numerical_integrator_function_call,char,fixed,rk4_numerical_integrator
   turbine_control_function_call,char,fixed,kw2_turbine_control
   eom_function_call,char,fixed,eom_simple_ball_thrown_in_air
   drivetrain_function_call,char,fixed,example_drivetrain
   flow_sim_model_function_call,char,fixed,example_flow_sim_model
   data_processing_function_call,char,fixed,example_data_processing
   qblade_interface_function_call,char,fixed,example_qblade_interface
   ```

2. **control_switch() Behavior**:
   - Reads each `*_function_call` string from `fixed_data`.  
   - Calls `DISPATCH_STAGE_OR_ERROR(..., <map>, <string>)` for each stage.  
   - If successful, `register_stage(...)` stores the function pointer.  
   - Subsequent calls to `stage_name(...)` use that function.

3. **Actual Invocation**:
   ```c
   // In main simulation loop:
   control_switch(dynamic_data, fixed_data);
   // Now every stage is registered.
   flow_gen(dynamic_data, fixed_data);               // calls csv_fixed_interp_flow_gen
   numerical_integrator(state_vars, state_names, ...);
   turbine_control(dynamic_data, fixed_data);
   eom(state_vars, state_names, ...);
   drivetrain(dynamic_data, fixed_data);
   flow_sim_model(dynamic_data, fixed_data);
   data_processing(dynamic_data, fixed_data, dp_args);
   qblade_interface(avr_swap, dynamic_data, fixed_data);
   ```

## Conditional Compilation for Libraries and Executables

- **`BUILD_SHARED_LIBS`**: Controls whether to build shared libraries or static libraries.  
  - Default: `ON`  
  - If `BUILD_XFE_CONTROL_SIM_EXECUTABLE` is also `ON`, shared libraries are disabled (for Windows builds).  

- **`BUILD_XFE_CONTROL_SIM_EXECUTABLE`**: Enables building the `xfe_control_sim` executable.  
  - Default: `ON`  

---

## To compile using Linux:

Run the `launch_test_program.sh` script located in `src/misc/`. This script will:

1. Invoke CMake to configure the build.  
2. Build the code using the chosen generator (e.g., Ninja or Make).  
3. Launch the resulting executable (typically `xfe_control_sim`).

If you do not have IWYU or CPPCHECK installed, you can comment out or remove those checks in the script. The basic dependencies are:

- **CMake**: version 3.15 or newer.  
- **A C compiler** (e.g., `gcc` or `clang`).  
- **Ninja**: Recommended for faster builds, but you can use `make` if preferred.

On Windows, ensure that Ninja and CMake are available in your `PATH` before running the script.

---

## Adding New Devices (e.g., Drivetrains, Flow Generators, etc.)

In this codebase, all related functions (e.g., all drivetrain variants) live in a single source file (for example, `src/drivetrains.c`). Function selection is driven at runtime by names read from the central configuration CSV. To add a new device (whether a drivetrain, a flow generator, or any stage-like component), follow these guidelines:

### 1. Implement Your New Function in the Consolidated File

1. **Open the consolidated source file** that contains similar implementations:  
   - For example, if you are adding a new drivetrain variant, edit `src/drivetrains.c`.  
   - If you are adding a new flow generator, edit `src/flow_gen.c`, and so on.

2. **Add your function** following the existing pattern:  
   - Each function must match the signature defined by the stage macro in its header.  
   - For instance, drivetrain functions have the signature:
     ```c
     void my_new_drivetrain(const param_array_t *dynamic_data,
                            const param_array_t *fixed_data);
     ```
   - Implement the body so that it reads whatever parameters it needs from `dynamic_data`/`fixed_data`, performs its calculations, and writes results back to `dynamic_data` before returning.

3. **Export the function name** in the corresponding header’s `Map` array:  
   - In `src/drivetrains.h`, find the `drivetrain_map[]` array and add an entry:
     ```c
     static const drivetrainMap drivetrain_map[] = {
         {"example_drivetrain", example_drivetrain},
         {"my_new_drivetrain",  my_new_drivetrain},
     };
     ```
   - This string (`"my_new_drivetrain"`) will be used in the CSV to select your implementation at runtime.

4. **Ensure the dispatch macro** is already present in `src/control_switch.c`:  
   - There should be a line like:
     ```c
     DEFINE_STAGE_DISPATCHER(drivetrain, drivetrain_map)
     ```
   - This allows the runtime to read the name from CSV, look it up in `drivetrain_map`, and register the function pointer.

By centralizing all variants in one file, you avoid creating multiple C files for each small variant. Instead, you append to a single `drivetrains.c` (or `flow_gen.c`, etc.).

---

### 2. Create or Update the Configuration CSV

1. **Locate your project’s main configuration CSV** (e.g., `simple_ball_config.csv` in `src/config/`).  
2. **Add a new line for your device** under the appropriate heading. For example, to select your new drivetrain:
   ```csv
   drivetrain_function_call,char,fixed,my_new_drivetrain
   ```
3. **Verify that other required parameters** (e.g., any fixed or dynamic values your function needs) are also present or added to this CSV file.  
   - If your new function reads a parameter like `drivetrain_efficiency`, add a line:
     ```csv
     drivetrain_efficiency,double,fixed,0.95
     ```
4. **When the program starts**, it reads every line of the CSV and populates the `param_array_t` structures. Then, in `control_switch()`, each `*_function_call` is retrieved by `get_param(fixed_data, "drivetrain_function_call", &drivetrain_function_name)`, and `DISPATCH_STAGE_OR_ERROR` registers the matching function pointer.

Because everything is driven by string names in the CSV, you do *not* need to reconfigure CMake or recompile C source paths. As long as the function is compiled into the library (by editing `drivetrains.c`), updating the CSV is sufficient to switch behavior.

---

### 3. No CMake Changes Needed for New Functions

Since all variants of a given stage (drivetrain, flow, integrator, etc.) are contained within a single source file per stage, you generally do not need to modify `config.cmake` or `CMakeLists.txt` when adding a new function. The build already compiles:

- `src/drivetrains.c`  
- `src/flow_gen.c`  
- `src/numerical_integrator.c`  
- etc.

Adding a new function to these files means the symbol is already included. At runtime, the CSV controls which variant is active. The only time you would touch CMake is if you:

- Introduce an entirely *new* stage (for example, a “filter” stage that did not exist), in which case you would:
  1. Create `src/filter.c` and `include/filter.h`.
  2. Add `filter.c` to the `LIB_SOURCES` list in `CMakeLists.txt`.
  3. Write a filter-specific `Map[]` in `filter.h` and add `DEFINE_STAGE_DISPATCHER(filter, filter_map)` to `control_switch.c`.

- Change the location of an existing single-file stage. In that case, update the file path in `CMakeLists.txt` accordingly.

---

### 4. Updating Feature Flags

Most compile-time flags (e.g., logging behavior, “run only one model” mode, or which config directory to use) are still set via `config.cmake`. You only need to adjust these if you want to change:  

- Where the program finds its CSV config (`SYSTEM_CONFIG_FULL_PATH`).  
- Whether logs are deleted on each run (`DELETE_LOG_FILE_NEW_RUN`).  
- Whether continuous dynamic logging is enabled (`LOGGING_DYNAMIC_DATA_CONTINUOUS`).  

If you add any entirely new logging requirements (for your new device), you would:  

1. Add a new flag name in `config.cmake` (e.g., `LOG_NEW_DEVICE_OUTPUT`).  
2. Pass it into `XFE_CONTROL_SIM_LIB_COMPILE_DEFINITIONS`.  
3. In code, wrap any logging calls with `#ifdef LOG_NEW_DEVICE_OUTPUT`.  

Generally, adding a new function variant does not require touching feature flags unless it needs its own compile-time toggle.

---

### 5. Verification Steps

1. **Edit your stage file** (e.g., `drivetrains.c`) to add `my_new_drivetrain(...)`.  
2. **Add the function name** (`my_new_drivetrain`) to the corresponding `Map[]` in `include/drivetrains.h`.  
3. **Update the configuration CSV** so that the line for `drivetrain_function_call` reads:
   ```csv
   drivetrain_function_call,char,fixed,my_new_drivetrain
   ```
4. **Run the launch script**:
   ```bash
   cd /path/to/xfe-control-sim
   ./src/misc/launch_test_program.sh
   ```
5. **Observe the log output**: In the beginning, `control_switch()` will print which function was selected. You can enable verbose logging to confirm that *your* function is registered and invoked.

If any of these steps fail, examine:

- **CSV parsing errors**: Check that the CSV has no stray commas or missing fields.  
- **Dispatcher errors**: If `DISPATCH_STAGE_OR_ERROR` fails, it will print a list of valid identifiers. Make sure your new function’s string exactly matches the entry in `Map[]`.  
- **Linker errors**: If your function is not referenced in `Map[]` or the symbol name is wrong, the build will fail.

---

## Additional Information

### Interface Notes
All interface functions are written in plain C to guarantee ABI stability. For turbine controller (e.g., SCADA testing), the code runs as a standalone executable that can be wrapped by a library or connected to other controllers via socket or shared memory.  

The consolidated file approach means that, for example, all drivetrains appear in `drivetrains.c` under a common function signature, and switching between them is simply a matter of changing a single CSV entry.

---

## CSV Structure

Here is an example of how entries in your configuration CSV might look:

```csv
variable_name,data_type,dynamic_or_fixed,value
log_file_location_and_or_name,char,fixed,xfe-control-sim-simulation-output.log
flow_function_call,char,fixed,csv_fixed_interp_flow_gen
numerical_integrator_function_call,char,fixed,rk4_numerical_integrator
... (other parameters) ...
drivetrain_function_call,char,fixed,my_new_drivetrain
```

- **`variable_name`**: The name that the program uses to lookup this parameter via `get_param(...)`.  
- **`data_type`**: One of (`char`, `double`, `int`, etc.).  
- **`dynamic_or_fixed`**: Indicates whether the parameter is read-only once at startup (`fixed`) or updated during the simulation (`dynamic`).  
- **`value`**: The literal value. For function names, this must match exactly one entry in the `Map[]` arrays.

You can add as many lines as needed to configure your new device or stage. When the program reads this CSV, it loads all entries into memory and then hands them off to each stage in turn.

---

## Using and Adapting `sim_example` for Customer Simulations

The `sim_example` folder serves as a standalone CMake-based demo project that illustrates how to integrate and run a simple simulation using the `xfe-control-sim` codebase. To adapt this example for a customer-specific simulation:

### 1. Directory Placement

- Place (or copy) a new `sim_example` folder at the same level as your `xfe-control-sim` repository. For example:
  ```
  /path/to/workspace/
  ├── xfe-control-sim/         (this repo)
  ├── sim_example/          (example project; copy & customize)
  └── customer_sim/         (your new, customized simulation)
  ```
- By default, `sim_example` expects to find `xfe-control-sim` in the sibling directory `../xfe-control-sim`. If `xfe-control-sim` is not present locally, `sim_example` will fetch it from GitHub during its CMake configuration.

### 2. Cloning or Copying `sim_example`

- If you already have the repository cloned, simply copy the existing `sim_example` folder to your workspace:
  ```bash
  cp -r /existing/path/xfe-control-sim/sim_example /path/to/workspace/
  ```
- Otherwise, clone `xfe-control-sim` (which contains `sim_example`) and then move or rename the example folder:
  ```bash
  git clone https://github.com/YourOrg/xfe-control-sim.git
  mv xfe-control-sim/sim_example /path/to/workspace/sim_example
  ```

### 3. Customizing the CMake Logic

- Open `sim_example/CMakeLists.txt`. Near the top, you will see logic that tries to locate `xfe-control-sim`:
  ```cmake
  if(NOT DEFINED XFE_CONTROL_SIM_ROOT)
    # Lookup sibling directory
    if(EXISTS "${CMAKE_SOURCE_DIR}/../xfe-control-sim/CMakeLists.txt")
      set(XFE_CONTROL_SIM_ROOT "${CMAKE_SOURCE_DIR}/../xfe-control-sim")
    else()
      # Fallback: clone from GitHub
      include(FetchContent)
      FetchContent_Declare(
        xfe_control_sim
        GIT_REPOSITORY https://github.com/YourOrg/xfe-control-sim.git
        GIT_TAG        main
      )
      FetchContent_MakeAvailable(xfe_control_sim)
      set(XFE_CONTROL_SIM_ROOT "${XFE_CONTROL_SIM_SOURCE_DIR}")
    endif()
  endif()

  # Now add_subdirectory(../xfe-control-sim) or the fetched content
  add_subdirectory(${XFE_CONTROL_SIM_ROOT} ${CMAKE_BINARY_DIR}/xfe-control-sim)
  ```
- If you rename or relocate the `xfe-control-sim` folder, update the relative path above. Alternatively, you can set the `XFE_CONTROL_SIM_ROOT` variable on the command line when configuring:
  ```bash
  cd sim_example
  mkdir build && cd build
  cmake -DXFE_CONTROL_SIM_ROOT=/path/to/xfe-control-sim ..
  ```
  This forces CMake to use the local copy rather than fetching from GitHub.

### 4. Adapting Sim Files

- The `sim_example` folder contains a minimal simulation driver (`src/flow_sim_model.c`, `src/turbine_control.c`, etc.) and a small `config/` directory with example CSV files. To create a customer simulation:
  1. **Duplicate the `sim_example` directory**. Rename it to something like `customer_sim`.
  2. **Replace or extend source files** in `customer_sim/src/` with your own implementations, following the same CMake and directory conventions. For example, if the customer needs a special turbine control, replace `turbine_control.c` with `customer_turbine_control.c` and adjust the `MakeStage` maps accordingly.
  3. **Adjust `config/` CSV files** under `customer_sim/config/` to contain the dynamic/fixed parameters for the customer’s machine (e.g., rotor properties, drivetrain ratios, wind data paths, etc.). Ensure the `*_function_call` entries match the stage identifiers in your source.
- Keep the same CMake targets and directory structure so that `customer_sim` remains a self-contained CMake project. The top-level `CMakeLists.txt` in `customer_sim` will include `add_subdirectory(${XFE_CONTROL_SIM_ROOT})` to pull in the core code.

### 5. Running the Example or Customer Simulation

- From within `sim_example` (or your `customer_sim`):
  ```bash
  mkdir -p build && cd build
  cmake ..              # Uses local xfe-control-sim or fetches if absent
  ninja                 # or `make` if Ninja is not installed
  ./executable_name     # Typically named after the directory (e.g., sim_example or customer_sim)
  ```
- CMake generates an executable (e.g., `sim_example` or `customer_sim`) in the build folder. You can pass arguments or change the CSV in `config/` to test different cases.

### 6. Version Control Strategy for Customers

- If you deliver `customer_sim` to a customer, you can either:
  1. Include a `.gitmodules` file pointing to the `xfe-control-sim` repo as a submodule, so that cloning `customer_sim` pulls in the correct commit of `xfe-control-sim`.
  2. Rely on the FetchContent logic to grab the right `xfe-control-sim` tag (e.g., `v1.2.3`) from GitHub.
- In both cases, document in your README which branch or tag of `xfe-control-sim` is required. If a customer modifies `customer_sim`, they can keep that as a separate Git repo without changing the core code in `xfe-control-sim`.

---

## Summary

- **`sim_example` is a standalone CMake project** that expects `xfe-control-sim` as a sibling. If missing, it fetches the code from GitHub automatically.  
- **Copy or rename `sim_example` to create a customer-specific simulation** (e.g., `customer_sim`).  
- **Edit source files** in `customer_sim/src` following the stage macro patterns (e.g., `MAKE_STAGE`, `Map[]`, `DISPATCH_STAGE_OR_ERROR`).  
- **Update the `config/*.csv` files** to reflect the customer’s machine parameters and to select which variant of each stage to run.  
- **Run via CMake** as an independent project. The same build commands (`cmake`, `ninja`/`make`) apply.  
- **Version control** can use either Git submodules or rely on the FetchContent fallback, ensuring reproducible builds.

By following these guidelines, you can quickly repurpose `sim_example` for any customer’s simulation, with minimal changes to the CMake logic and no need to merge back upstream unless you introduce entirely new stages.
```
