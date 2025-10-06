#!/usr/bin/env python3
"""
Shared utility functions for build and formatting scripts.

This module provides common functionality used across multiple scripts:
- Color/emoji output handling
- CI environment detection
- File searching utilities
- Command existence checking
- Common build/format/analysis runners
"""

import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional, Union

class Colors:
	"""ANSI color codes for terminal output."""
	RED = '\033[91m'
	GREEN = '\033[92m'
	YELLOW = '\033[93m'
	BLUE = '\033[94m'
	RESET = '\033[0m'
	BOLD = '\033[1m'

	@staticmethod
	def disable():
		"""Disable colors (useful for non-TTY environments)."""
		Colors.RED = Colors.GREEN = Colors.YELLOW = Colors.BLUE = ''
		Colors.BOLD = Colors.RESET = ''

class Emoji:
	"""Emoji characters for output."""
	CHECK = '✅ '
	CROSS = '❌ '
	WARNING = '⚠️ '
	INFO = 'ℹ️ '

	@staticmethod
	def disable():
		"""Disable emojis for environments that don't support them."""
		Emoji.CHECK = '[OK]'
		Emoji.CROSS = '[FAIL]'
		Emoji.WARNING = '[WARN]'
		Emoji.INFO = '[INFO]'

def is_ci_environment() -> bool:
	"""Detect if running in CI/CD environment."""
	ci_vars = [
	    'CI', 'CONTINUOUS_INTEGRATION', 'GITHUB_ACTIONS', 'GITLAB_CI', 'CIRCLECI', 'TRAVIS', 'JENKINS_HOME',
	    'TEAMCITY_VERSION'
	]
	return any(var in os.environ for var in ci_vars)

def get_git_root(exit_on_error: bool = True) -> Optional[Path]:
	"""
    Get the root directory of the Git repository.
    
    Args:
        exit_on_error: If True, exit with error message on failure.
                      If False, return None on failure.
    
    Returns:
        Path to git root as Path object, or None if not found (when exit_on_error=False).
    """
	try:
		result = subprocess.run(['git', 'rev-parse', '--show-toplevel'], capture_output=True, text=True, check=True)
		return Path(result.stdout.strip())
	except subprocess.CalledProcessError:
		if exit_on_error:
			print(f"{Colors.RED}Error: Not in a git repository{Colors.RESET}", file=sys.stderr)
			sys.exit(1)
		return None
	except FileNotFoundError:
		if exit_on_error:
			print(f"{Colors.RED}Error: Git not found in PATH{Colors.RESET}", file=sys.stderr)
			sys.exit(1)
		return None

def setup_display():
	"""Configure colors and emojis based on environment."""
	if not sys.stdout.isatty() or (os.name == 'nt' and not os.environ.get('ANSICON')):
		Colors.disable()

	if is_ci_environment() or (os.name == 'nt' and sys.stdout.encoding not in ['utf-8', 'UTF-8']):
		Emoji.disable()

def find_file(project_root: Union[str, Path],
              script_name: str,
              env_var_name: str,
              max_depth: int = 3) -> Optional[Path]:
	"""
    Find a file within a project by searching in prioritized locations.

    Args:
        project_root: The root directory of the project to search within.
        script_name: The name of the file to find.
        env_var_name: Environment variable that can override the search.
        max_depth: Maximum number of subdirectories to search.

    Returns:
        Path object to the file or None if not found.
    """
	project_root = Path(project_root).resolve()

	if env_var_name in os.environ:
		script_path_str = os.environ[env_var_name]
		script_path = Path(script_path_str)
		if script_path.is_file():
			print(f"-> Found '{script_name}' via {env_var_name} environment variable.")
			return script_path
		else:
			print(
			    f"{Colors.YELLOW}Warning: {env_var_name} set to '{script_path_str}' "
			    f"but not found.{Colors.RESET}",
			    file=sys.stderr)

	print(f"-> Searching for '{script_name}' up to {max_depth} levels deep "
	      f"from '{project_root}'...")

	root_depth = len(project_root.parts)

	for current_dir, dirs, files in os.walk(project_root):
		current_path = Path(current_dir)
		depth = len(current_path.parts) - root_depth

		if script_name in files:
			found_path = current_path / script_name
			print(f"   Found: {found_path}")
			return found_path

		if depth >= max_depth:
			dirs[:] = []

	print(f"   '{script_name}' not found within the specified depth.")
	return None

def check_command_exists(cmd: str) -> bool:
	"""Check if a command exists in PATH."""
	result = shutil.which(cmd)
	if result:
		return True

	# On Windows, also check for extension-less files (like run-clang-tidy)
	if platform.system() == 'Windows':
		# Try with .py extension
		if shutil.which(f"{cmd}.py"):
			return True

		# Manually search PATH for extension-less files
		paths = os.environ.get('PATH', '').split(os.pathsep)
		for path_dir in paths:
			full_path = os.path.join(path_dir, cmd)
			if os.path.isfile(full_path):
				return True

	return False

def run_clang_format(repo_root: Path, search_depth: int = 4) -> bool:
	"""Run clang-format on all source files by finding the helper script."""
	repo_root = Path(repo_root)
	script_to_find = "clang_format_all.py"

	clang_format_script = find_file(
	    project_root=str(repo_root),
	    script_name=script_to_find,
	    env_var_name="CLANG_FORMAT_SCRIPT_PATH",
	    max_depth=search_depth)

	if not clang_format_script:
		print(f"{Colors.YELLOW}{Emoji.WARNING} Could not find '{script_to_find}' "
		      f"script; skipping.{Colors.RESET}")
		return True

	if not check_command_exists("clang-format"):
		print(
		    f"{Colors.YELLOW}{Emoji.WARNING} clang-format not found in PATH; "
		    f"skipping clang-format step.{Colors.RESET}")
		return True

	print(f"-> Running clang-format script: {clang_format_script}")
	result = subprocess.run([sys.executable, str(clang_format_script)], cwd=repo_root, capture_output=True, text=True)

	if result.returncode != 0:
		print(
		    f"{Colors.YELLOW}{Emoji.WARNING} clang-format script returned non-zero "
		    f"exit code: {result.returncode}{Colors.RESET}")
		if result.stdout:
			print("--- stdout ---")
			print(result.stdout.strip())
		if result.stderr:
			print("--- stderr ---")
			print(result.stderr.strip())
		return False
	else:
		print(f"{Emoji.CHECK} clang-format completed successfully")
		return True

def find_run_clang_tidy() -> Optional[str]:
	"""
    Find the 'run-clang-tidy' script.

    Checks environment variable first, then intelligently searches for
    'clang-tidy' binary to locate 'run-clang-tidy' alongside it.

    Returns:
        Path to the 'run-clang-tidy' executable or None if not found.
    """
	env_path = os.environ.get('RUN_CLANG_TIDY_BIN')
	if env_path:
		if os.path.exists(env_path):
			return env_path
		else:
			print(
			    f"{Colors.YELLOW}Warning: RUN_CLANG_TIDY_BIN set to '{env_path}' "
			    f"but not found{Colors.RESET}",
			    file=sys.stderr)

	# Try to find clang-tidy and look in its directory
	clang_tidy_path = shutil.which('clang-tidy')
	if clang_tidy_path:
		clang_tidy_dir = Path(clang_tidy_path).parent
		candidates = ['run-clang-tidy', 'run-clang-tidy.py', 'run-clang-tidy.exe']

		for candidate in candidates:
			run_clang_tidy_script = clang_tidy_dir / candidate
			if run_clang_tidy_script.is_file():
				return str(run_clang_tidy_script)

	# Try direct which() lookup
	run_clang_tidy_direct = shutil.which('run-clang-tidy')
	if run_clang_tidy_direct:
		return run_clang_tidy_direct

	# On Windows, manually search PATH for extension-less file
	if platform.system() == 'Windows':
		paths = os.environ.get('PATH', '').split(os.pathsep)
		for path_dir in paths:
			# Try without extension
			full_path = os.path.join(path_dir, 'run-clang-tidy')
			if os.path.isfile(full_path):
				return full_path
			# Try with .py
			full_path_py = os.path.join(path_dir, 'run-clang-tidy.py')
			if os.path.isfile(full_path_py):
				return full_path_py

	return None

def run_clang_tidy(
        repo_root: Path, build_dir: Path, mode: str = 'c', in_cloud: bool = False, search_depth: int = 4) -> bool:
	"""Run clang-tidy analysis."""
	repo_root = Path(repo_root)
	build_dir = Path(build_dir)
	script_to_find = "clang_tidy_all.py"

	clang_tidy_script = find_file(
	    project_root=str(repo_root),
	    script_name=script_to_find,
	    env_var_name="CLANG_TIDY_SCRIPT_PATH",
	    max_depth=search_depth)

	if not clang_tidy_script:
		print(f"{Colors.YELLOW}{Emoji.WARNING} '{script_to_find}' script not found; "
		      f"skipping.{Colors.RESET}")
		return True

	run_clang_tidy_bin = find_run_clang_tidy()
	if not run_clang_tidy_bin:
		print(
		    f"{Colors.YELLOW}{Emoji.WARNING} run-clang-tidy not found in PATH; "
		    f"skipping clang-tidy step.{Colors.RESET}")
		return True

	env = os.environ.copy()
	env['PROJECT_ROOT'] = str(repo_root)
	env['BUILD_DIR'] = str(build_dir)

	# Always pass just the path to the binary
	# clang_tidy_all.py will handle calling it with Python if needed
	env['RUN_CLANG_TIDY_BIN'] = run_clang_tidy_bin

	clang_tidy_log = build_dir / 'clang-tidy.log'

	if clang_tidy_log.exists():
		clang_tidy_log.unlink()

	print(f"\n{Emoji.INFO} Running clang-tidy ({mode})... logging to {clang_tidy_log}")

	cmd = [sys.executable, str(clang_tidy_script), mode]

	if in_cloud:
		cmd.extend(['--extraargs', '-warnings-as-errors=*'])

	try:
		with open(clang_tidy_log, 'w', encoding='utf-8') as log_file:
			process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env)

			for line in process.stdout:
				print(line, end='')
				log_file.write(line)

			process.wait()

			if process.returncode != 0:
				print(
				    f"{Colors.YELLOW}{Emoji.WARNING} clang-tidy returned non-zero; "
				    f"see {clang_tidy_log}{Colors.RESET}")
				return False

			return True

	except Exception as e:
		print(f"{Colors.RED}{Emoji.CROSS} Error running clang-tidy: {e}{Colors.RESET}", file=sys.stderr)
		return False

def run_yapf(repo_root: Path, search_depth: int = 4) -> bool:
	"""Run yapf to format all Python files in the repository."""
	repo_root = Path(repo_root)

	is_github_actions = os.environ.get("GITHUB_ACTIONS") == "true"
	is_ci = is_github_actions or os.environ.get("CI") is not None

	if is_ci:
		print("Not running yapf since we are in CI")
		return True

	if not check_command_exists("yapf"):
		print(f"{Colors.YELLOW}{Emoji.WARNING} yapf not found in PATH; "
		      f"skipping Python formatting.{Colors.RESET}")
		return True

	print("-> Running yapf to format Python files...")
	result = subprocess.run(["yapf", "-i", "-r", "."], cwd=repo_root, capture_output=True, text=True)

	if result.returncode != 0:
		print(
		    f"{Colors.YELLOW}{Emoji.WARNING} yapf returned non-zero exit code: "
		    f"{result.returncode}{Colors.RESET}")
		if result.stderr:
			print(result.stderr)
		return False
	else:
		print(f"{Emoji.CHECK} yapf completed successfully")
		return True
