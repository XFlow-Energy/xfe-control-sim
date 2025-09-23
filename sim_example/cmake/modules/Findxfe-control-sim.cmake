# -----------------------------------------------------------------------------
# SPDX-License-Identifier: CC0-1.0
#
# This file is part of the XFLOW-CONTROL-SIM example suite.
#
# To the extent possible under law, XFlow Energy has waived all copyright
# and related or neighboring rights to this example file. This work is
# published from: United States.
#
# You should have received a copy of the CC0 Public Domain Dedication along
# with this software. If not, see <https://creativecommons.org/publicdomain/zero/1.0/>.
# -----------------------------------------------------------------------------

# Findxflow-control-sim.cmake

# If we've already imported the sim-include interface, bail out immediately:
if(TARGET xflow-control-sim-include)
	message(STATUS "xflow-control-sim already configured; skipping Find module")
	set(XFLOW_CONTROL_SIM_FOUND TRUE)
	return()
endif()

# Optional: cap how far we walk up (0 = only current dir). Default: unlimited.
if(NOT DEFINED XFE_MAX_PARENT_DEPTH)
	set(XFE_MAX_PARENT_DEPTH 0) # -1 means "no limit"
endif()

# Prefer branch in GitHub Actions cloud by default (can be disabled)
if(NOT DEFINED XFLOW_CONTROL_SIM_PREFER_BRANCH_IN_CLOUD)
	set(XFLOW_CONTROL_SIM_PREFER_BRANCH_IN_CLOUD ON)
endif()

# Branch knob (can be overridden by -DXFLOW_CONTROL_SIM_BRANCH=...)
# NOTE: This is used when no tag is selected/resolved.
if(NOT DEFINED XFLOW_CONTROL_SIM_BRANCH)
	set(XFLOW_CONTROL_SIM_BRANCH "jason_working")
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
if(_IN_CLOUD AND _CLOUD_REPO_NAME_LC STREQUAL "xflow-control-sim" AND XFLOW_CONTROL_SIM_PREFER_BRANCH_IN_CLOUD)
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
		set(_CLOUD_BRANCH "${XFLOW_CONTROL_SIM_BRANCH}")
	endif()
endif()

# Discover a local checkout if LOCAL_XFLOW_CONTROL_SIM_DIR isn't supplied
if (NOT LOCAL_XFLOW_CONTROL_SIM_DIR)
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
			set(_candidate "${_dir}/xflow-control-sim")
			# message(STATUS "Checking parent directory: ${_candidate}")

			if (EXISTS "${_candidate}/CMakeLists.txt")
				set(LOCAL_XFLOW_CONTROL_SIM_DIR "${_candidate}")
				set(_xfe_found TRUE)
				message(STATUS "Found local xflow-control-sim at ${LOCAL_XFLOW_CONTROL_SIM_DIR}")
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
			"Could not find a local xflow-control-sim by walking parents of:\n"
			"  ${CMAKE_CURRENT_SOURCE_DIR}\n"
			"  ${CMAKE_SOURCE_DIR}\n"
			"Checked each parent with suffix '/xflow-control-sim'.\n"
			"Set -DLOCAL_XFLOW_CONTROL_SIM_DIR=… to point at your checkout."
		)
	endif()
endif()

message(STATUS "LOCAL_XFLOW_CONTROL_SIM_DIR = ${LOCAL_XFLOW_CONTROL_SIM_DIR}")

# ---------------- Token resolution policy ----------------
# Look for .secrets one folder up from the current project folder
set(_secrets_file "${CMAKE_SOURCE_DIR}/../.secrets")
if(EXISTS "${_secrets_file}")
    file(STRINGS "${_secrets_file}" _secret_lines)
    foreach(_line IN LISTS _secret_lines)
        if(_line MATCHES "^[ \t]*XFLOW_CONTROL_SIM_TOKEN[ \t]*=")
            string(REGEX REPLACE "^[^=]*=" "" HARDCODED_XFLOW_CONTROL_SIM_TOKEN "${_line}")
            string(STRIP "${HARDCODED_XFLOW_CONTROL_SIM_TOKEN}" HARDCODED_XFLOW_CONTROL_SIM_TOKEN)
        endif()
    endforeach()
endif()

# 1) Prefer explicit env var (custom secret you define): XFLOW_CONTROL_SIM_TOKEN
# 2) Then GitHub's built-in GITHUB_TOKEN (export it in your workflow step)
# 3) Then an optional cache var XFLOW_CONTROL_SIM_DIST_TOKEN (manual override)
# 4) Finally (LOCAL ONLY) the hardcoded token
set(_token "")

if(DEFINED ENV{XFLOW_CONTROL_SIM_TOKEN} AND NOT "$ENV{XFLOW_CONTROL_SIM_TOKEN}" STREQUAL "")
    set(_token "$ENV{XFLOW_CONTROL_SIM_TOKEN}")
elseif(DEFINED ENV{GITHUB_TOKEN} AND NOT "$ENV{GITHUB_TOKEN}" STREQUAL "")
    set(_token "$ENV{GITHUB_TOKEN}")
elseif(DEFINED XFLOW_CONTROL_SIM_DIST_TOKEN AND NOT "${XFLOW_CONTROL_SIM_DIST_TOKEN}" STREQUAL "")
    set(_token "${XFLOW_CONTROL_SIM_DIST_TOKEN}")
endif()

# In CI, do NOT fall back to the hardcoded token (avoid leakage/misuse).
if(_IN_CLOUD)
    if("${_token}" STREQUAL "")
        message(FATAL_ERROR
            "GitHub Actions detected but no token provided. "
            "Set env XFLOW_CONTROL_SIM_TOKEN or GITHUB_TOKEN in the workflow.")
    endif()
else()
    # Local build: allow hardcoded as absolute last resort
    if("${_token}" STREQUAL "")
        set(_token "${HARDCODED_XFLOW_CONTROL_SIM_TOKEN}")
    endif()
endif()

# Helper for printing a redacted URL (never echo the token).
set(_token_redacted "***")

# if (EXISTS "${LOCAL_XFLOW_CONTROL_SIM_DIR}/.git")
#     message(STATUS "Found local xflow-control-sim at ${LOCAL_XFLOW_CONTROL_SIM_DIR}")

# 	# Per requirement: do NOT change branches; just fetch + fast-forward pull.
# 	execute_process(COMMAND git -C "${LOCAL_XFLOW_CONTROL_SIM_DIR}" fetch)
# 	message(STATUS "Pulling latest changes for xflow-control-sim (no branch changes)")
# 	execute_process(COMMAND git -C "${LOCAL_XFLOW_CONTROL_SIM_DIR}" pull --ff-only)

# 	# Bring it into this build
# 	add_subdirectory("${LOCAL_XFLOW_CONTROL_SIM_DIR}" "xflow-control-sim")

# else()
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

	message(STATUS "Fetching xflow-control-sim via FetchContent")
	include(FetchContent)

	# --- Version/Tag resolution (optional) ---
	# If XFLOW_CONTROL_SIM_VERSION is "" or "latest", resolve to the latest release tag via GitHub API.
	# If set to a non-empty value, verify that tag exists via the releases/tags/<tag> endpoint.
	# If a tag is resolved/provided, it overrides the branch.
	# In GitHub Actions (cloud), if XFLOW_CONTROL_SIM_PREFER_BRANCH_IN_CLOUD=ON, prefer the current branch.
	set(_use_tag FALSE)
	unset(_resolved_tag)

	# Cloud override: prefer current branch, skip tag resolution
	set(_cloud_prefer_branch FALSE)
	if(_IN_CLOUD AND XFLOW_CONTROL_SIM_PREFER_BRANCH_IN_CLOUD)
		set(_cloud_prefer_branch TRUE)
	endif()

	if(NOT _cloud_prefer_branch)
		if(DEFINED XFLOW_CONTROL_SIM_VERSION)
			if(XFLOW_CONTROL_SIM_VERSION STREQUAL "" OR XFLOW_CONTROL_SIM_VERSION STREQUAL "latest")
				# Need token + curl to query latest release
				if(NOT DEFINED XFLOW_CONTROL_SIM_DIST_TOKEN OR XFLOW_CONTROL_SIM_DIST_TOKEN STREQUAL "")
					# fallback to hardcoded if separate DIST token not provided
					set(XFLOW_CONTROL_SIM_DIST_TOKEN "${HARDCODED_XFLOW_CONTROL_SIM_TOKEN}")
				endif()
				find_program(CURL_EXECUTABLE curl REQUIRED)

				set(_api_json "${CMAKE_BINARY_DIR}/xflow-control-sim-release.json")
				execute_process(
					COMMAND "${CURL_EXECUTABLE}"
						-sS -L
						-H "Authorization: Bearer ${XFLOW_CONTROL_SIM_DIST_TOKEN}"
						-H "User-Agent: xflow-control-sim-cmake"
						-H "Accept: application/vnd.github+json"
						-H "X-GitHub-Api-Version: 2022-11-28"
						"https://api.github.com/repos/XFlow-Energy/xflow-control-sim/releases/latest"
						-o "${_api_json}"
						-w "%{http_code}"
					RESULT_VARIABLE _curl_rv
					OUTPUT_VARIABLE _http_code
					ERROR_VARIABLE _curl_err
				)
				string(STRIP "${_http_code}" _http_code)
				if(NOT _curl_rv EQUAL 0 OR NOT _http_code MATCHES "^200$")
					if(EXISTS "${_api_json}")
						file(READ "${_api_json}" _err_body)
						message(FATAL_ERROR "GitHub API query failed for latest release (rv=${_curl_rv}, http=${_http_code}). curl error: ${_curl_err}\nResponse: ${_err_body}")
					else()
						message(FATAL_ERROR "GitHub API query failed for latest release (rv=${_curl_rv}, http=${_http_code}). curl error: ${_curl_err}")
					endif()
				endif()

				file(READ "${_api_json}" _gh_content)
				if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.19")
					string(JSON _resolved_tag GET "${_gh_content}" tag_name)
				else()
					string(REGEX MATCH "\"tag_name\"[ \t]*:[ \t]*\"([^\"]+)\"" _m "${_gh_content}")
					set(_resolved_tag "${CMAKE_MATCH_1}")
				endif()

				if(NOT _resolved_tag)
					message(FATAL_ERROR "Failed to parse tag_name from GitHub API (latest).")
				endif()

				set(_use_tag TRUE)
				message(STATUS "Resolved xflow-control-sim latest tag: ${_resolved_tag}")

			else()
				# Specific tag requested: verify it exists
				if(NOT DEFINED XFLOW_CONTROL_SIM_DIST_TOKEN OR XFLOW_CONTROL_SIM_DIST_TOKEN STREQUAL "")
					set(XFLOW_CONTROL_SIM_DIST_TOKEN "${HARDCODED_XFLOW_CONTROL_SIM_TOKEN}")
				endif()
				find_program(CURL_EXECUTABLE curl REQUIRED)

				set(_api_json "${CMAKE_BINARY_DIR}/xflow-control-sim-release-${XFLOW_CONTROL_SIM_VERSION}.json")
				execute_process(
					COMMAND "${CURL_EXECUTABLE}"
						-sS -L
						-H "Authorization: Bearer ${XFLOW_CONTROL_SIM_DIST_TOKEN}"
						-H "User-Agent: xflow-control-sim-cmake"
						-H "Accept: application/vnd.github+json"
						-H "X-GitHub-Api-Version: 2022-11-28"
						"https://api.github.com/repos/XFlow-Energy/xflow-control-sim/releases/tags/${XFLOW_CONTROL_SIM_VERSION}"
						-o "${_api_json}"
						-w "%{http_code}"
					RESULT_VARIABLE _curl_rv_t
					OUTPUT_VARIABLE _http_code_t
					ERROR_VARIABLE _curl_err_t
				)
				string(STRIP "${_http_code_t}" _http_code_t)
				if(NOT _curl_rv_t EQUAL 0 OR NOT _http_code_t MATCHES "^200$")
					if(_http_code_t STREQUAL "404")
						message(FATAL_ERROR "Requested xflow-control-sim tag not found: '${XFLOW_CONTROL_SIM_VERSION}' (http=404).")
					endif()
					if(EXISTS "${_api_json}")
						file(READ "${_api_json}" _err_body_t)
						message(FATAL_ERROR "GitHub API query failed for tag '${XFLOW_CONTROL_SIM_VERSION}' (rv=${_curl_rv_t}, http=${_http_code_t}). curl error: ${_curl_err_t}\nResponse: ${_err_body_t}")
					else()
						message(FATAL_ERROR "GitHub API query failed for tag '${XFLOW_CONTROL_SIM_VERSION}' (rv=${_curl_rv_t}, http=${_http_code_t}). curl error: ${_curl_err_t}")
					endif()
				endif()

				file(READ "${_api_json}" _gh_content_t)
				if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.19")
					string(JSON _parsed_tag GET "${_gh_content_t}" tag_name)
				else()
					string(REGEX MATCH "\"tag_name\"[ \t]*:[ \t]*\"([^\"]+)\"" _m2 "${_gh_content_t}")
					set(_parsed_tag "${CMAKE_MATCH_1}")
				endif()

				if(NOT _parsed_tag)
					message(FATAL_ERROR "Failed to parse tag_name for requested tag '${XFLOW_CONTROL_SIM_VERSION}'.")
				endif()
				if(NOT _parsed_tag STREQUAL "${XFLOW_CONTROL_SIM_VERSION}")
					message(FATAL_ERROR "Requested tag '${XFLOW_CONTROL_SIM_VERSION}' but API returned '${_parsed_tag}'.")
				endif()

				set(_resolved_tag "${XFLOW_CONTROL_SIM_VERSION}")
				set(_use_tag TRUE)
				message(STATUS "Using requested xflow-control-sim tag: ${_resolved_tag}")
			endif()
		endif()
	else()
		# Cloud override path: force branch to the cloud branch and skip tag resolution
		if(NOT _CLOUD_BRANCH STREQUAL "")
			set(XFLOW_CONTROL_SIM_BRANCH "${_CLOUD_BRANCH}")
			message(STATUS "GitHub Actions detected; preferring branch '${XFLOW_CONTROL_SIM_BRANCH}' over release tag")
		else()
			message(STATUS "GitHub Actions detected but no branch name resolved; using '${XFLOW_CONTROL_SIM_BRANCH}'")
		endif()
	endif()

	# Use GitHub token over HTTPS for git fetches
	# GitHub recommends the "x-access-token" username form
	if(NOT "${_token}" STREQUAL "")
		set(XFLOW_CONTROL_SIM_REPO_URL "https://x-access-token:${_token}@github.com/XFlow-Energy/xflow-control-sim.git")
		set(_repo_url_print "https://x-access-token:${_token_redacted}@github.com/XFlow-Energy/xflow-control-sim.git")
	else()
		# Public fallback (no auth). If the repo is private, this will fail loudly — by design.
		set(XFLOW_CONTROL_SIM_REPO_URL "https://github.com/XFlow-Energy/xflow-control-sim.git")
		set(_repo_url_print "${XFLOW_CONTROL_SIM_REPO_URL}")
	endif()

	message(STATUS "xflow-control-sim repo URL selected (redacted): ${_repo_url_print}")

	# Choose what to pass to FetchContent: tag (preferred if resolved) or branch
	if(_use_tag)
		set(_git_ref "${_resolved_tag}")
		message(STATUS "FetchContent will checkout tag: ${_git_ref}")
	else()
		set(_git_ref "${XFLOW_CONTROL_SIM_BRANCH}")
		message(STATUS "FetchContent will checkout branch: ${_git_ref}")
	endif()

	FetchContent_Declare(
		xflow_control_sim
		GIT_REPOSITORY ${XFLOW_CONTROL_SIM_REPO_URL}
		GIT_TAG        ${_git_ref}
		GIT_SHALLOW    TRUE
		GIT_PROGRESS   TRUE
	)
	FetchContent_MakeAvailable(xflow_control_sim)

	# Restore FETCHCONTENT_BASE_DIR if we temporarily overrode a cached value
	if(_had_old_fetchcontent)
		unset(FETCHCONTENT_BASE_DIR)  # kill our normal var
		set(FETCHCONTENT_BASE_DIR "${_old_FETCHCONTENT_BASE_DIR}"
			CACHE PATH "FetchContent base dir" FORCE)
		message(STATUS "✓ Restored FETCHCONTENT_BASE_DIR='${FETCHCONTENT_BASE_DIR}'")
		unset(_old_FETCHCONTENT_BASE_DIR)
		unset(_had_old_fetchcontent)
	endif()
# endif()

# If the project defines its usual include interface target, we can mark FOUND.
if(TARGET xflow-control-sim-include)
	set(XFLOW_CONTROL_SIM_FOUND TRUE)
else()
	# Fall back to TRUE if the project added itself differently but didn't provide the expected target.
	# Adjust to FALSE if you want a strict check.
	set(XFLOW_CONTROL_SIM_FOUND TRUE)
endif()