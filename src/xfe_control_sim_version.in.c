/**
 * @file    xfe_control_sim_version.c
 * @author  XFlow Energy
 * @date    2025
 * @brief   XFE-CONTROL-SIM Git version.
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

/* *************************************************************
@CONFIGURE_WARNING@
// *************************************************************/

#include "xfe_control_sim_version.h"

// the dollar signs and Revision are so you can run rcs ident on a binary
// @git_commit_info_xfe_control_sim_warning@
#define GIT_COMMIT_INFO_XFE_CONTROL_SIM "$$Revision: @git_commit_info_xfe_control_sim@ $$"

// include git info in built binaries
// you should be able to get it with `ident` or `strings | grep '$$Revision'`.
// (note the single quotes so the shell doesn't try to expand a variable named $$R)
// building with any optimization should deduplicate this, but it depends on the compiler and options and such
const char gitCommitInfoXfeControlSim[] = GIT_COMMIT_INFO_XFE_CONTROL_SIM;
