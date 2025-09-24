/**
 * @file    bladed_interface.h
 * @author  XFlow Energy
 * @date    2025
 * @brief   Defines for bladed interface
 */

/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * XFE-CONTROL-SIM
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

#ifndef BLADED_INTERFACE_H
#define BLADED_INTERFACE_H

#ifdef __APPLE__
// ARM or ARM64 specific headers
#include <arm/signal.h> // for sig_atomic_t
#elifdef _WIN32
typedef int sig_atomic_t;
#endif

#include "xfe_control_sim_common.h"
#include "xflow_core.h"
#include "xfe_control_sim_version.h"
#include <time.h>
#include <stdint.h>
#include <signal.h>

/* Bladed Interface Index Definitions */

/* Data Flow: in (Variables passed from the simulation to the controller) */

#define REC_CURRENT_TIME 1                     /* Current time (s) */
#define REC_COMMUNICATION_INTERVAL 2           /* Communication interval (s) */
#define REC_BLADE1_PITCH_ANGLE 3               /* Blade 1 pitch angle (rad) */
#define REC_BELOW_RATED_PITCH_ANGLE_SETPOINT 4 /* Below-rated pitch angle set-point (rad) */
#define REC_MIN_PITCH_ANGLE 5                  /* Minimum pitch angle (rad) */
#define REC_MAX_PITCH_ANGLE 6                  /* Maximum pitch angle (rad) */
#define REC_MIN_PITCH_RATE 7                   /* Minimum pitch rate (most negative value allowed) (rad/s) */
#define REC_MAX_PITCH_RATE 8                   /* Maximum pitch rate (rad/s) */
#define REC_PITCH_ACTUATOR_TYPE 9              /* 0 = pitch position actuator, 1 = pitch rate actuator */
#define REC_CURRENT_DEMANDED_PITCH_ANGLE 10    /* Current demanded pitch angle (rad) */
#define REC_CURRENT_DEMANDED_PITCH_RATE 11     /* Current demanded pitch rate (rad/s) */
#define REC_DEMANDED_POWER 12                  /* Demanded power (W) */
#define REC_MEASURED_SHAFT_POWER 13            /* Measured shaft power (W) */
#define REC_MEASURED_ELECTRICAL_POWER 14       /* Measured electrical power output (W) */
#define REC_OPTIMAL_MODE_GAIN 15               /* Optimal mode gain (Nm/(rad/s)^2) */
#define REC_MIN_GENERATOR_SPEED 16             /* Minimum generator speed (rad/s) */
#define REC_OPTIMAL_MODE_MAX_SPEED 17          /* Optimal mode maximum speed (rad/s) */
#define REC_DEMANDED_GEN_SPEED_ABOVE_RATED 18  /* Demanded generator speed above rated (rad/s) */
#define REC_MEASURED_GENERATOR_SPEED 19        /* Measured generator speed (rad/s) */
#define REC_MEASURED_ROTOR_SPEED 20            /* Measured rotor speed (rad/s) */
#define REC_DEMANDED_GEN_TORQUE_ABOVE_RATED 21 /* Demanded generator torque above rated (Nm) */
#define REC_MEASURED_GENERATOR_TORQUE 22       /* Measured generator torque (Nm) */
#define REC_MEASURED_YAW_ERROR 23              /* Measured yaw error (rad) */
#define REC_TORQUE_SPEED_TABLE_START 24        /* Start of below-rated torque-speed look-up table */
#define REC_TORQUE_SPEED_TABLE_POINTS 25       /* Number of points in torque-speed look-up table */
#define REC_HUB_WIND_SPEED 26                  /* Hub wind speed (m/s) */
#define REC_PITCH_CONTROL_TYPE 27              /* Pitch control: 0 = collective, 1 = individual */
#define REC_YAW_CONTROL_TYPE 28                /* Yaw control: 0 = yaw rate control, 1 = yaw torque control */
#define REC_BLADE2_PITCH_ANGLE 29              /* Blade 2 pitch angle (rad) */
#define REC_BLADE1_ROOT_OP_BENDING_MOMENT 30   /* Blade 1 root out-of-plane bending moment (Nm) */
#define REC_BLADE2_ROOT_OP_BENDING_MOMENT 31   /* Blade 2 root out-of-plane bending moment (Nm) */
#define REC_BLADE3_ROOT_OP_BENDING_MOMENT 32   /* Blade 3 root out-of-plane bending moment (Nm) */
#define REC_BLADE3_PITCH_ANGLE 33              /* Blade 3 pitch angle (rad) */
#define REC_GENERATOR_CONTACTOR 34             /* Generator contactor */
#define REC_SHAFT_BRAKE_STATUS 35              /* Shaft brake status: 0=off, 1=Brake 1 on */
#define REC_NACELLE_ANGLE_FROM_NORTH 36        /* Nacelle angle from North (rad) */
#define REC_PITCH_OVERRIDE 54                  /* Pitch override */
#define REC_TOWER_TOP_FA_ACCELERATION 53       /* Tower top fore-aft acceleration (m/s^2) */
#define REC_ROTOR_AZIMUTH_ANGLE 59             /* Rotor azimuth angle (rad) */
#define REC_NUMBER_OF_BLADES 60                /* Number of blades */
#define REC_MAX_LOGGING_VALUES 61              /* Max. number of values which can be returned for logging */
#define REC_LOGGING_START_RECORD 62            /* Record number for start of logging output */
#define REC_MAX_OUTNAME_CHARS 63               /* Max. number of characters which can be returned in OUTNAME */
#define REC_NUMBER_OF_LOGGING_VARIABLES 64     /* Number of variables returned for logging */
#define REC_BLADE1_ROOT_IP_BENDING_MOMENT 68   /* Blade 1 root in-plane bending moment (Nm) */
#define REC_BLADE2_ROOT_IP_BENDING_MOMENT 69   /* Blade 2 root in-plane bending moment (Nm) */
#define REC_BLADE3_ROOT_IP_BENDING_MOMENT 70   /* Blade 3 root in-plane bending moment (Nm) */
#define REC_GENERATOR_STARTUP_RESISTANCE 71    /* Generator start-up resistance (ohm/phase) */
#define REC_ROTATING_HUB_MY 72                 /* Rotating hub My (Nm) */
#define REC_ROTATING_HUB_MZ 73                 /* Rotating hub Mz (Nm) */
#define REC_FIXED_HUB_MY 74                    /* Fixed hub My (Nm) */
#define REC_FIXED_HUB_MZ 75                    /* Fixed hub Mz (Nm) */
#define REC_YAW_BEARING_MY 76                  /* Yaw bearing My (Nm) */
#define REC_YAW_BEARING_MZ 77                  /* Yaw bearing Mz (Nm) */
#define REC_REQUEST_FOR_LOADS 78               /* Request for loads */
#define REC_VARIABLE_SLIP_CURRENT_FLAG 79      /* Variable slip current flag */
#define REC_VARIABLE_SLIP_CURRENT 80           /* Variable slip current demand (A) */
#define REC_NACELLE_ROLL_ACCELERATION 81       /* Nacelle roll acceleration (rad/s^2) */
#define REC_NACELLE_NOD_ACCELERATION 82        /* Nacelle nodding acceleration (rad/s^2) */
#define REC_NACELLE_YAW_ACCELERATION 83        /* Nacelle yaw acceleration (rad/s^2) */
#define REC_REAL_TIME_SIMULATION_TIME_STEP 89  /* Real-time simulation time step (s) */
#define REC_REAL_TIME_STEP_MULTIPLIER 90       /* Real-time simulation time step multiplier */
#define REC_MEAN_WIND_SPEED_INCREMENT 91       /* Mean wind speed increment (m/s) */
#define REC_TURBULENCE_INTENSITY_INCREMENT 92  /* Turbulence intensity increment (%) */
#define REC_WIND_DIRECTION_INCREMENT 93        /* Wind direction increment (rad) */
#define REC_SAFETY_SYSTEM_ACTIVATED 96         /* Safety system number that has been activated */
#define REC_SAFETY_SYSTEM_TO_ACTIVATE 97       /* Safety system number to activate */
#define REC_YAW_CONTROL_FLAG 101               /* Yaw control flag */
#define REC_YAW_STIFFNESS 102                  /* Yaw stiffness if REC_YAW_CONTROL_FLAG = 1 or 3 */
#define REC_YAW_DAMPING 103                    /* Yaw damping if REC_YAW_CONTROL_FLAG = 2 or 3 */
#define REC_BRAKE_TORQUE_DEMAND 106            /* Brake torque demand (Nm) */
#define REC_YAW_BRAKE_TORQUE_DEMAND 107        /* Yaw brake torque demand (Nm) */
#define REC_SHAFT_TORQUE 108                   /* Shaft torque (Nm) */
#define REC_HUB_FIXED_FX 109                   /* Hub Fixed Fx (N) */
#define REC_HUB_FIXED_FY 110                   /* Hub Fixed Fy (N) */
#define REC_HUB_FIXED_FZ 111                   /* Hub Fixed Fz (N) */
#define REC_NETWORK_VOLTAGE_DISTURBANCE 112    /* Network voltage disturbance factor */
#define REC_NETWORK_FREQUENCY_DISTURBANCE 113  /* Network frequency disturbance factor */
#define REC_CONTROLLER_STATE 116               /* Controller state */
#define REC_SETTLING_TIME 117                  /* Settling time (s) */
#define REC_TEETER_ANGLE 142                   /* Teeter angle (rad) */
#define REC_TEETER_VELOCITY 143                /* Teeter velocity (rad/s) */
#define REC_CONTROLLER_FAILURE_FLAG 160        /* Controller failure flag */
#define REC_YAW_BEARING_POSITION 161           /* Yaw bearing angular position (rad) */
#define REC_YAW_BEARING_VELOCITY 162           /* Yaw bearing angular velocity (rad/s) */
#define REC_YAW_BEARING_ACCELERATION 163       /* Yaw bearing angular acceleration (rad/s^2) */

/* Data Flow: out (Variables passed from the controller to the simulation) */

#define REC_DEMANDED_YAW_TORQUE 40            /* Demanded yaw actuator torque (Nm) */
#define REC_DEMANDED_BLADE1_PITCH 41          /* Demanded blade 1 individual pitch (rad or rad/s) */
#define REC_DEMANDED_BLADE2_PITCH 42          /* Demanded blade 2 individual pitch (rad or rad/s) */
#define REC_DEMANDED_BLADE3_PITCH 43          /* Demanded blade 3 individual pitch (rad or rad/s) */
#define REC_DEMANDED_COLLECTIVE_PITCH 44      /* Demanded collective pitch angle (rad) */
#define REC_DEMANDED_COLLECTIVE_PITCH_RATE 45 /* Demanded collective pitch rate (rad/s) */
#define REC_DEMANDED_GENERATOR_TORQUE 46      /* Demanded generator torque (Nm) */
#define REC_DEMANDED_NACELLE_YAW_RATE 47      /* Demanded nacelle yaw rate (rad/s) */
#define REC_MESSAGE_LENGTH 48                 /* Message length or -M0 */

/* Data Flow: both (Variables used for two-way communication) */

#define REC_PITCH_OVERRIDE 54  /* Pitch override */
#define REC_TORQUE_OVERRIDE 55 /* Torque override */
/* User-defined variables */
#define REC_USER_VARIABLE_1 119  /* User-defined variable 1 */
#define REC_USER_VARIABLE_2 120  /* User-defined variable 2 */
#define REC_USER_VARIABLE_3 121  /* User-defined variable 3 */
#define REC_USER_VARIABLE_4 122  /* User-defined variable 4 */
#define REC_USER_VARIABLE_5 123  /* User-defined variable 5 */
#define REC_USER_VARIABLE_6 124  /* User-defined variable 6 */
#define REC_USER_VARIABLE_7 125  /* User-defined variable 7 */
#define REC_USER_VARIABLE_8 126  /* User-defined variable 8 */
#define REC_USER_VARIABLE_9 127  /* User-defined variable 9 */
#define REC_USER_VARIABLE_10 128 /* User-defined variable 10 */

/* Notes:
 * - Indices have been adjusted by subtracting 1 to account for C's zero-based indexing.
 * - Variables are grouped by Data Flow: 'in', 'out', and 'both'.
 * - Data types: 'I' indicates Integer, 'R' indicates Real (floating-point).
 * - Ensure that when accessing integer variables, proper casting is performed
 *   since the array is of type double.
 */
#endif
