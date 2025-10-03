/**
 * @file    discon.h
 * @author  XFlow Energy
 * @date    2025
 * @brief   API for DISCON for QBlade
 */

/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * This file is part of the XFE-CONTROL-SIM example suite.
 *
 * To the extent possible under law, XFlow Energy has waived all copyright
 * and related or neighboring rights to this example file. This work is
 * published from: United States.
 *
 * You should have received a copy of the CC0 Public Domain Dedication along
 * with this software. If not, see <https://creativecommons.org/publicdomain/zero/1.0/>.
 */

#ifndef DISCON_H
#define DISCON_H

#include "xflow_core.h"
#include "xfe_control_sim_common.h"
#include "xfe_control_sim_version.h"
#include "xfe_control_sim_lib_export.h"

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef _WIN32
#ifndef __cdecl
#define __cdecl
#endif
#endif

#define DISCON_PARAM_LIST MAYBE_UNUSED float *avr_swap, MAYBE_UNUSED int *avi_fail, MAYBE_UNUSED char *acc_in_file, MAYBE_UNUSED char *avc_outname, MAYBE_UNUSED char *avc_msg
#define DISCON_DISPATCH_ARGS avr_swap, avi_fail, acc_in_file, avc_outname, avc_msg
#define DISCON_CALL_ARGS avr_swap, &avi_fail, acc_in_file, avc_outname, avc_msg

typedef void (*DISCON_fn)(DISCON_PARAM_LIST);

void register_DISCON(DISCON_fn fn);

XFE_CONTROL_SIM_LIB_EXPORT void __cdecl DISCON(DISCON_PARAM_LIST);

typedef struct
{
	const char *id;
	DISCON_fn fn;
} DISCON_Map;

void example_discon(DISCON_PARAM_LIST);

static const DISCON_Map disconMap[] = {
	{"example_discon", example_discon},
};

#endif
