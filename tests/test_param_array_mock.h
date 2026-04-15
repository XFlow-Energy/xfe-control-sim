/**
 * @file    test_param_array_mock.h
 * @brief   Mock implementations of param_array functions for testing
 * @author  XFlow Energy
 * @date    2025
 */

/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef TEST_PARAM_ARRAY_MOCK_H
#define TEST_PARAM_ARRAY_MOCK_H

#include "xflow_aero_sim.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Initialize a param_array structure
 */
static inline void init_param_array(param_array_t *data)
{
	data->n_param = 0;
	data->params = NULL;
}

/**
 * @brief Free all resources in a param_array
 */
static inline void free_param_array(param_array_t *data)
{
	if (data->params)
	{
		for (int i = 0; i < data->n_param; i++)
		{
			if (data->params[i].name)
			{
				free(data->params[i].name);
			}
			if (data->params[i].type == INPUT_PARAM_STRING && data->params[i].value.s)
			{
				free(data->params[i].value.s);
			}
			if (data->params[i].history)
			{
				// Free history buffer if allocated
				if (data->params[i].type == INPUT_PARAM_INT && data->params[i].history->data.i_hist)
				{
					free(data->params[i].history->data.i_hist);
				}
				else if (data->params[i].type == INPUT_PARAM_DOUBLE && data->params[i].history->data.d_hist)
				{
					free(data->params[i].history->data.d_hist);
				}
				else if (data->params[i].type == INPUT_PARAM_STRING && data->params[i].history->data.s_hist)
				{
					free(data->params[i].history->data.s_hist);
				}
				free(data->params[i].history);
			}
		}
		free(data->params);
	}
	data->n_param = 0;
	data->params = NULL;
}

/**
 * @brief Add a double parameter to the array
 */
static inline void add_param_double(param_array_t *data, const char *name, double value, bool is_state_var)
{
	// Reallocate array
	data->params = (input_param_t *)realloc(data->params, sizeof(input_param_t) * (data->n_param + 1));

	// Initialize new parameter
	data->params[data->n_param].name = strdup(name);
	data->params[data->n_param].type = INPUT_PARAM_DOUBLE;
	data->params[data->n_param].is_state_var = is_state_var;
	data->params[data->n_param].value.d = value;
	data->params[data->n_param].update_time_sec = 0.0;
	data->params[data->n_param].history = NULL;

	data->n_param++;
}

/**
 * @brief Add an integer parameter to the array
 */
static inline void add_param_int(param_array_t *data, const char *name, int value, bool is_state_var)
{
	data->params = (input_param_t *)realloc(data->params, sizeof(input_param_t) * (data->n_param + 1));

	data->params[data->n_param].name = strdup(name);
	data->params[data->n_param].type = INPUT_PARAM_INT;
	data->params[data->n_param].is_state_var = is_state_var;
	data->params[data->n_param].value.i = value;
	data->params[data->n_param].update_time_sec = 0.0;
	data->params[data->n_param].history = NULL;

	data->n_param++;
}

/**
 * @brief Add a string parameter to the array
 */
static inline void add_param_string(param_array_t *data, const char *name, const char *value, bool is_state_var)
{
	data->params = (input_param_t *)realloc(data->params, sizeof(input_param_t) * (data->n_param + 1));

	data->params[data->n_param].name = strdup(name);
	data->params[data->n_param].type = INPUT_PARAM_STRING;
	data->params[data->n_param].is_state_var = is_state_var;
	data->params[data->n_param].value.s = strdup(value);
	data->params[data->n_param].update_time_sec = 0.0;
	data->params[data->n_param].history = NULL;

	data->n_param++;
}

/**
 * @brief Get a double parameter by name
 */
static inline const double *get_param_double(const param_array_t *data, const char *name)
{
	for (int i = 0; i < data->n_param; i++)
	{
		if (strcmp(data->params[i].name, name) == 0 && data->params[i].type == INPUT_PARAM_DOUBLE)
		{
			return &data->params[i].value.d;
		}
	}
	return NULL;
}

/**
 * @brief Get an integer parameter by name
 */
static inline const int *get_param_int(const param_array_t *data, const char *name)
{
	for (int i = 0; i < data->n_param; i++)
	{
		if (strcmp(data->params[i].name, name) == 0 && data->params[i].type == INPUT_PARAM_INT)
		{
			return &data->params[i].value.i;
		}
	}
	return NULL;
}

/**
 * @brief Get a string parameter by name
 */
static inline const char *get_param_string(const param_array_t *data, const char *name)
{
	for (int i = 0; i < data->n_param; i++)
	{
		if (strcmp(data->params[i].name, name) == 0 && data->params[i].type == INPUT_PARAM_STRING)
		{
			return data->params[i].value.s;
		}
	}
	return NULL;
}

// Override get_param macro from xflow_aero_sim.h with our mock version.
#ifdef get_param
#undef get_param
#endif

// Implementation: looks up by name, dispatches on the stored type, and writes
// a pointer of the matching type into *value. Mirrors the real
// get_param_impl in xflow-utils.
//   double param  -> *(const double **)value  = &stored.value.d   (mutable)
//   int    param  -> *(const int    **)value  = &stored.value.i   (mutable)
//   string param  -> *(const char   **)value  =  stored.value.s   (the char *)
static inline void get_param_mock_impl(const param_array_t *data, const char *name, void *value)
{
	if (!value)
	{
		return;
	}
	for (int i = 0; i < data->n_param; i++)
	{
		if (strcmp(data->params[i].name, name) != 0)
		{
			continue;
		}
		switch (data->params[i].type)
		{
		case INPUT_PARAM_DOUBLE:
			*(const double **)value = &data->params[i].value.d;
			return;
		case INPUT_PARAM_INT:
			*(const int **)value = &data->params[i].value.i;
			return;
		case INPUT_PARAM_STRING:
			*(const char **)value = data->params[i].value.s;
			return;
		default:
			return;
		}
	}
}

// Wrap as a macro with an explicit (void *) cast of the caller's typed
// out-pointer. This mirrors xflow-utils's real get_param macro and lets
// callers pass `&typed_ptr` without a multi-level pointer conversion warning.
#define get_param(pa, name, out_ptr) get_param_mock_impl((pa), (name), (void *)(out_ptr)) // NOLINT(readability-identifier-naming)

#endif // TEST_PARAM_ARRAY_MOCK_H
