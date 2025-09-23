/**
 * @file    numerical_integrator.h
 * @author  XFlow Energy
 * @date    2025
 * @brief   API for numerical_integrator
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
#ifndef MAKE_STAGE_H
#define MAKE_STAGE_H

#include "logger.h"     // for ERROR_MESSAGE (if you need it elsewhere)
#include "xflow_core.h" // for shutdownFlag (if you need it elsewhere)

/*
 *   MAKE_STAGE(name, RTYPE, PARAMS, ...)
 *
 *   - declares name##_fn, register_name(), and name()
 *   - typedefs a struct name##_Map { const char *id; name##_fn fn; }
 */
#define MAKE_STAGE(name, RTYPE, PARAMS)                                        \
	typedef RTYPE(*name##_fn) PARAMS; /* NOLINT(bugprone-macro-parentheses) */ \
	void register_##name(name##_fn fn);                                        \
	RTYPE name PARAMS;                                                         \
	typedef struct                                                             \
	{                                                                          \
		const char *id;                                                        \
		name##_fn fn;                                                          \
	} name##_Map;

/*
 * definition macro (for exactly one .c file).
 *   - PARAMS: full "(type name, type name, ...)"
 *   - ARGS:   bare "(name, name, ...)" for the call
 */
#define MAKE_STAGE_DEFINE(name, RTYPE, PARAMS, ARGS)                                     \
	static name##_fn name##_cb = NULL;                                                   \
	void register_##name(name##_fn fn)                                                   \
	{                                                                                    \
		name##_cb = fn;                                                                  \
	}                                                                                    \
                                                                                         \
	static RTYPE default_##name PARAMS                                                   \
	{                                                                                    \
		(void)sizeof((ARGS)); /* NOLINT(bugprone-sizeof-expression,cert-arr39-c) */      \
		log_message("We should not be in here..., default_" #name ", ending program\n"); \
		shutdownFlag = 1;                                                                \
	}                                                                                    \
                                                                                         \
	__attribute__((constructor(101))) static void init_default_##name(void)              \
	{                                                                                    \
		register_##name(default_##name);                                                 \
	}                                                                                    \
                                                                                         \
	RTYPE name PARAMS                                                                    \
	{                                                                                    \
		if (name##_cb)                                                                   \
			name##_cb ARGS;                                                              \
		else                                                                             \
		{                                                                                \
			(void)sizeof((ARGS)); /* NOLINT(bugprone-sizeof-expression,cert-arr39-c) */  \
			ERROR_MESSAGE(#name "_cb function pointer not declared.\n");                 \
			shutdownFlag = 1;                                                            \
		}                                                                                \
	}
// 1) Defines a static dispatcher function for <name>, looking it up
//    in the map <map_array>.  The map must be an array of:
//
//      typedef struct { const char *id; name##_fn fn; } name##_Map;
//      extern const name##_Map map_array[];
//
#define DEFINE_STAGE_DISPATCHER(name, map_array)                                               \
	static bool dispatch_##name(const char *which)                                             \
	{                                                                                          \
		size_t _n = sizeof(map_array) / sizeof((map_array)[0]);                                \
		for (size_t _i = 0; _i < _n; ++_i)                                                     \
			if (strcmp(which, map_array[_i].id) == 0) /* NOLINT(bugprone-macro-parentheses) */ \
			{                                                                                  \
				register_##name(map_array[_i].fn); /* NOLINT(bugprone-macro-parentheses) */    \
				return true;                                                                   \
			}                                                                                  \
		return false;                                                                          \
	}

// 2) Inlines that dispatcher and emits an error if not found:
// calls that dispatcher, and on failure prints ALL the valid map IDs
#define DISPATCH_STAGE_OR_ERROR(name, map_array, which_str)                                                \
	do                                                                                                     \
	{                                                                                                      \
		if (!dispatch_##name(which_str))                                                                   \
		{                                                                                                  \
			ERROR_MESSAGE("Unknown " #name "_call '%s'\n", which_str);                                     \
			log_message("Valid " #name "_call options:");                                                  \
			size_t _n = sizeof(map_array) / sizeof((map_array)[0]);                                        \
			for (size_t _j = 0; _j < _n; ++_j)                                                             \
				safe_fwritef(stderr, "    %s", map_array[_j].id); /* NOLINT(bugprone-macro-parentheses) */ \
			safe_fwritef(stderr, "\n");                                                                    \
			shutdownFlag = 1;                                                                              \
		}                                                                                                  \
	} while (0)

#endif // MAKE_STAGE_H
