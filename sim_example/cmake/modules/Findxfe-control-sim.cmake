# -----------------------------------------------------------------------------
# SPDX-License-Identifier: CC0-1.0
#
# This file is part of the XFE-CONTROL-SIM example suite.
#
# To the extent possible under law, XFlow Energy has waived all copyright
# and related or neighboring rights to this example file. This work is
# published from: United States.
#
# You should have received a copy of the CC0 Public Domain Dedication along
# with this software. If not, see <https://creativecommons.org/publicdomain/zero/1.0/>.
# -----------------------------------------------------------------------------

# Findxfe-control-sim.cmake

# If we've already imported the sim-include interface, bail out immediately:
if(TARGET xfe-control-sim-include)
	message(STATUS "xfe-control-sim already configured; skipping Find module")
	set(XFE_CONTROL_SIM_FOUND TRUE)
	return()
endif()

# Optional: cap how far we walk up (0 = only current dir). Default: unlimited.
if(NOT DEFINED XFE_MAX_PARENT_DEPTH)
	set(XFE_MAX_PARENT_DEPTH 8) # -1 means "no limit"
endif()

# Prefer branch in GitHub Actions cloud by default (can be disabled)
if(NOT DEFINED XFE_CONTROL_SIM_PREFER_BRANCH_IN_CLOUD)
	set(XFE_CONTROL_SIM_PREFER_BRANCH_IN_CLOUD ON)
endif()

# Branch knob (can be overridden by -DXFE_CONTROL_SIM_BRANCH=...)
# NOTE: This is used when no tag is selected/resolved.
if(NOT DEFINED XFE_CONTROL_SIM_BRANCH)
	set(XFE_CONTROL_SIM_BRANCH "working")
endif()

# Detect if we're in GitHub Actions and, if so, derive the branch name
set(_IN_CLOUD FALSE)
if(DEFINED ENV{GITHUB_ACTIONS} AND "$ENV{GITHUB_ACTIONS}" STREQUAL "true")
	set(_IN_CLOUD TRUE)
endif()

# Determine the repository name (owner/repo → repo) for gating
set(_CLOUD_REPO_NAME "")
if(_IN_CLOUD)
	if(DEFINED ENV{GITHUB_REPOSITORY} AND NOT "$ENV{GITHUB_REPOSITORY}" STREQUAL "")
		set(_tmp_repo "$ENV{GITHUB_REPOSITORY}")
		string(REGEX REPLACE ".*/" "" _CLOUD_REPO_NAME "${_tmp_repo}")
	elseif(DEFINED ENV{GITHUB_REPOSITORY_NAME} AND NOT "$ENV{GITHUB_REPOSITORY_NAME}" STREQUAL "")
		set(_CLOUD_REPO_NAME "$ENV{GITHUB_REPOSITORY_NAME}")
	endif()
endif()

set(_CLOUD_BRANCH "")
string(TOLOWER "${_CLOUD_REPO_NAME}" _CLOUD_REPO_NAME_LC)
if(_IN_CLOUD AND _CLOUD_REPO_NAME_LC STREQUAL "xfe-control-sim" AND XFE_CONTROL_SIM_PREFER_BRANCH_IN_CLOUD)
	# Priority: PR source branch -> direct push branch -> ref name -> fallback
	if(DEFINED ENV{GITHUB_HEAD_REF} AND NOT "$ENV{GITHUB_HEAD_REF}" STREQUAL "")
		set(_CLOUD_BRANCH "$ENV{GITHUB_HEAD_REF}")
	elseif(DEFINED ENV{GITHUB_REF} AND "$ENV{GITHUB_REF}" MATCHES "^refs/heads/.+")
		string(REGEX REPLACE "^refs/heads/" "" _CLOUD_BRANCH "$ENV{GITHUB_REF}")
	elseif(DEFINED ENV{GITHUB_REF_NAME} AND NOT "$ENV{GITHUB_REF_NAME}" STREQUAL "")
		# GitHub provides GITHUB_REF_NAME in many contexts (branch or tag name)
		# Use it only if it's not a tag context later.
		set(_CLOUD_BRANCH "$ENV{GITHUB_REF_NAME}")
	endif()
	if(_CLOUD_BRANCH STREQUAL "")
		set(_CLOUD_BRANCH "${XFE_CONTROL_SIM_BRANCH}")
	endif()
endif()

# Discover a local checkout if LOCAL_XFE_CONTROL_SIM_DIR isn't supplied
if (NOT LOCAL_XFE_CONTROL_SIM_DIR)
	# Build ordered roots: current dir first, then project root if different.
	set(_xfe_roots "${CMAKE_CURRENT_SOURCE_DIR}")
	if (NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
		list(APPEND _xfe_roots "${CMAKE_SOURCE_DIR}")
	endif()

	set(_xfe_found FALSE)

	foreach(_root IN LISTS _xfe_roots)
		if (_xfe_found)
			break()
		endif()

		set(_dir "${_root}")
		set(_depth 0)
		while(TRUE)
			set(_candidate "${_dir}/xfe-control-sim")
			# message(STATUS "Checking parent directory: ${_candidate}")

			if (EXISTS "${_candidate}/CMakeLists.txt")
				set(LOCAL_XFE_CONTROL_SIM_DIR "${_candidate}")
				set(_xfe_found TRUE)
				message(STATUS "Found local xfe-control-sim at ${LOCAL_XFE_CONTROL_SIM_DIR}")
				break()
			endif()

			get_filename_component(_parent "${_dir}" DIRECTORY)
			if (_parent STREQUAL _dir)
				break()
			endif()

			math(EXPR _depth "${_depth} + 1")
			if (NOT XFE_MAX_PARENT_DEPTH EQUAL -1 AND _depth GREATER_EQUAL XFE_MAX_PARENT_DEPTH)
				message(STATUS "Reached XFE_MAX_PARENT_DEPTH=${XFE_MAX_PARENT_DEPTH} at ${_dir}; stopping ascent for this root.")
				break()
			endif()

			set(_dir "${_parent}")
		endwhile()
	endforeach()

	# Fallback: if still not found, keep the old advisory message.
	if (NOT _xfe_found)
		message(STATUS
			"Could not find a local xfe-control-sim by walking parents of:\n"
			"  ${CMAKE_CURRENT_SOURCE_DIR}\n"
			"  ${CMAKE_SOURCE_DIR}\n"
			"Checked each parent with suffix '/xfe-control-sim'.\n"
			"Set -DLOCAL_XFE_CONTROL_SIM_DIR=… to point at your checkout."
		)
	endif()
endif()

message(STATUS "LOCAL_XFE_CONTROL_SIM_DIR = ${LOCAL_XFE_CONTROL_SIM_DIR}")

# If a local checkout exists, use it directly
if (EXISTS "${LOCAL_XFE_CONTROL_SIM_DIR}/CMakeLists.txt")
    message(STATUS "Using local xfe-control-sim at ${LOCAL_XFE_CONTROL_SIM_DIR}")

    # Optional: fast-forward only (safe) pull if the directory is a git repo.
    if (EXISTS "${LOCAL_XFE_CONTROL_SIM_DIR}/.git")
        execute_process(COMMAND git -C "${LOCAL_XFE_CONTROL_SIM_DIR}" fetch
                        RESULT_VARIABLE _ff_fetch_rv)
        if(_ff_fetch_rv EQUAL 0)
            message(STATUS "Pulling latest changes for xfe-control-sim (no branch changes)")
            execute_process(COMMAND git -C "${LOCAL_XFE_CONTROL_SIM_DIR}" pull --ff-only)
        endif()
    endif()

    add_subdirectory("${LOCAL_XFE_CONTROL_SIM_DIR}" "xfe-control-sim")

else()
    # If the build-script passed FETCHCONTENT_BASE_DIR=/tmp/cmakedeps, ignore it:
    if(DEFINED FETCHCONTENT_BASE_DIR AND FETCHCONTENT_BASE_DIR STREQUAL "/tmp/cmakedeps")
        set(_old_FETCHCONTENT_BASE_DIR "${FETCHCONTENT_BASE_DIR}")
        set(_had_old_fetchcontent TRUE)
        unset(FETCHCONTENT_BASE_DIR CACHE)  # remove the nasty cache override
        message(STATUS "⧗ Ignoring FETCHCONTENT_BASE_DIR='/tmp/cmakedeps' for this fetch")
    endif()

    # 2) Inject a *normal* var so FetchContent puts things under your build
    #    (no CACHE here, so it won’t persist beyond this scope)
    set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps")
    message(STATUS "→ Temporarily using FETCHCONTENT_BASE_DIR='${FETCHCONTENT_BASE_DIR}'")

    message(STATUS "Fetching xfe-control-sim via FetchContent (public)")

    include(FetchContent)

    # --- Public repo: no tokens, no API calls ---
    # Decide which ref to use:
    #   - If XFE_CONTROL_SIM_VERSION is a non-empty, non-"latest" string → use that tag/branch
    #   - Else if we're in CI and we resolved a branch from the workflow → use that branch
    #   - Else fall back to the configured default branch
    if(DEFINED XFE_CONTROL_SIM_VERSION
       AND NOT XFE_CONTROL_SIM_VERSION STREQUAL ""
       AND NOT XFE_CONTROL_SIM_VERSION STREQUAL "latest")
        set(_git_ref "${XFE_CONTROL_SIM_VERSION}")
        message(STATUS "xfe-control-sim: using ref '${_git_ref}' (public)")
    elseif(_IN_CLOUD AND XFE_CONTROL_SIM_PREFER_BRANCH_IN_CLOUD AND NOT _CLOUD_BRANCH STREQUAL "")
        set(_git_ref "${_CLOUD_BRANCH}")
        message(STATUS "xfe-control-sim: CI detected; using branch '${_git_ref}' (public)")
    else()
        set(_git_ref "${XFE_CONTROL_SIM_BRANCH}")
        message(STATUS "xfe-control-sim: using default branch '${_git_ref}' (public)")
    endif()

    # Public HTTPS repo URL (no auth needed)
    set(XFE_CONTROL_SIM_REPO_URL "https://github.com/XFlow-Energy/xfe-control-sim.git")
    message(STATUS "xfe-control-sim repo URL: ${XFE_CONTROL_SIM_REPO_URL}")

    # Fetch
    FetchContent_Declare(
        xfe_control_sim
        GIT_REPOSITORY ${XFE_CONTROL_SIM_REPO_URL}
        GIT_TAG        ${_git_ref}
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
    )
    FetchContent_MakeAvailable(xfe_control_sim)

    # Restore FETCHCONTENT_BASE_DIR if we temporarily overrode a cached value
    if(_had_old_fetchcontent)
        unset(FETCHCONTENT_BASE_DIR)  # kill our normal var
        set(FETCHCONTENT_BASE_DIR "${_old_FETCHCONTENT_BASE_DIR}"
            CACHE PATH "FetchContent base dir" FORCE)
        message(STATUS "✓ Restored FETCHCONTENT_BASE_DIR='${FETCHCONTENT_BASE_DIR}'")
        unset(_old_FETCHCONTENT_BASE_DIR)
        unset(_had_old_fetchcontent)
    endif()
endif()

# If the project defines its usual include interface target, we can mark FOUND.
if(TARGET xfe-control-sim-include)
    set(XFE_CONTROL_SIM_FOUND TRUE)
else()
    # Fall back to TRUE if the project added itself differently but didn't provide the expected target.
    # Adjust to FALSE if you want a strict check.
    set(XFE_CONTROL_SIM_FOUND TRUE)
endif()