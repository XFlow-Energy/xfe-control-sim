#!/usr/bin/env python3
"""
A cross-platform Python script to run clang-tidy.

This script is a replacement for the original bash script, designed to work on
Windows, macOS, and Linux without any external libraries.

It determines the project root, build directory, and clang-tidy binary from
environment variables or sensible defaults. It parses command-line arguments
to set the analysis mode (c, cpp, all), specify a .clang-tidy file, and
pass extra arguments to the clang-tidy runner.

The script automatically detects the number of CPU cores for parallel execution
and, on macOS, finds the system SDK path to include as a compiler flag.
It finds source files based on the selected mode and then executes
'run-clang-tidy' with all the collected arguments.
"""
import os
import sys
import subprocess
import platform
import shutil
from pathlib import Path

def find_run_clang_tidy():
	"""
    Finds the 'run-clang-tidy' script.

    It checks the environment variable first, then intelligently searches
    for the 'clang-tidy' binary to locate 'run-clang-tidy' alongside it.

    Returns:
        str: The path to the 'run-clang-tidy' executable or the default name.
    """
	# 1. Prioritize the environment variable if it's set and valid.
	env_path = os.environ.get('RUN_CLANG_TIDY_BIN')
	if env_path and os.path.exists(env_path):
		return env_path

	# 2. Find the main 'clang-tidy' binary to infer the path.
	clang_tidy_path = shutil.which('clang-tidy')
	if clang_tidy_path:
		# Construct the expected path for run-clang-tidy in the same directory.
		# e.g., C:/llvm/bin/clang-tidy.exe -> C:/llvm/bin/run-clang-tidy
		run_clang_tidy_script = Path(clang_tidy_path).parent / 'run-clang-tidy'
		if run_clang_tidy_script.is_file():
			# Return the full, verified path.
			return str(run_clang_tidy_script)

	# 3. Fall back to the default name and let the OS search the PATH.
	return 'run-clang-tidy'

def find_source_files(mode, project_root, build_dir):
	"""
    Finds source files in the current directory based on the mode.

    Args:
        mode (str): The file selection mode ('c', 'cpp', 'all', or 'both').

    Returns:
        list: A list of file paths matching the mode.
    """
	c_ext = ('.c',)
	cpp_ext = ('.cc', '.cpp', '.cxx')
	all_ext = c_ext + cpp_ext

	extensions = {'c': c_ext, 'cpp': cpp_ext, 'all': all_ext, 'both': all_ext}

	target_extensions = extensions.get(mode)
	if not target_extensions:
		return None

	found_files = []
	build_dir_name = os.path.basename(build_dir)
	for root, dirs, files in os.walk(project_root):
		if build_dir_name in dirs:
			dirs.remove(build_dir_name)
		for f in files:
			if f.endswith(target_extensions):
				relative_path = os.path.relpath(os.path.join(root, f), start=project_root)
				found_files.append(relative_path)
	return found_files

def main():
	"""Main execution function."""
	# 1. Set up initial variables from environment or defaults
	project_root = os.environ.get('PROJECT_ROOT', os.getcwd())
	build_dir = os.environ.get('BUILD_DIR', os.path.join(project_root, 'build'))
	run_clang_tidy_bin = find_run_clang_tidy()

	# 2. Parse command-line arguments
	args = sys.argv[1:]

	mode = 'all'
	if args and args[0] in ['c', 'cpp', 'both', 'all']:
		mode = args.pop(0)

	clang_tidy_file = os.path.join(project_root, '.clang-tidy')
	user_extra_args = []

	if args and args[0] != '--extraargs':
		clang_tidy_file = args.pop(0)

	if args and args[0] == '--extraargs':
		args.pop(0)
		user_extra_args = args

	# Change to the project root directory
	os.chdir(project_root)

	# 3. Build the base arguments for run-clang-tidy
	try:
		# os.cpu_count() is the cross-platform way to get core count
		nproc = os.cpu_count() or 1
	except NotImplementedError:
		nproc = 1

	# Pass the build_dir with OS-native separators.
	extra_args = [f'-j{nproc}', f'-p={build_dir}']

	# Add macOS-specific SDK path if applicable
	if platform.system() == 'Darwin':
		try:
			sdk_path_result = subprocess.run(['xcrun', '--show-sdk-path'], capture_output=True, text=True, check=True)
			sdk_path = sdk_path_result.stdout.strip()
			if sdk_path:
				extra_args.append(f'-extra-arg=-isysroot{sdk_path}')
		except (FileNotFoundError, subprocess.CalledProcessError):
			# xcrun might not be available or might fail
			pass

	# Add config file argument if it exists
	if os.path.isfile(clang_tidy_file):
		extra_args.append(f'-config-file={clang_tidy_file}')
	else:
		print(f"[WARN] Config file not found: {clang_tidy_file}", file=sys.stderr)

	files_to_process = find_source_files(mode, project_root, build_dir)

	if files_to_process is None:
		sys.exit(2)

	files_to_process = [os.path.abspath(p) for p in files_to_process]

	# --- FINAL CHANGE ---
	# On Windows, escape backslashes for the regex inside run-clang-tidy.py
	if platform.system() == "Windows":
		files_to_process = [p.replace("\\", "\\\\") for p in files_to_process]

	if files_to_process:
		print(f"DEBUG: First file path SENT to clang-tidy:       '{files_to_process[0]}'")

	if platform.system() == "Windows":
		final_command = [sys.executable, run_clang_tidy_bin, *extra_args, *user_extra_args, *files_to_process]
	else:
		final_command = [run_clang_tidy_bin, *extra_args, *user_extra_args, *files_to_process]

	print(f"[clang-tidy] {' '.join(final_command)}")

	try:
		subprocess.run(final_command, check=True)
	except subprocess.CalledProcessError as e:
		sys.exit(e.returncode)
	except OSError as e:
		print(f"Error executing command: {e}", file=sys.stderr)
		sys.exit(1)

if __name__ == "__main__":
	main()
