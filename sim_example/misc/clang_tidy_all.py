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

def find_source_files(mode):
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
	for root, _, files in os.walk('.'):
		for f in files:
			if f.endswith(target_extensions):
				found_files.append(os.path.join(root, f))
	return found_files

def main():
	"""Main execution function."""
	# 1. Set up initial variables from environment or defaults
	project_root = os.environ.get('PROJECT_ROOT', os.getcwd())
	build_dir = os.environ.get('BUILD_DIR', os.path.join(project_root, 'build'))
	run_clang_tidy_bin = os.environ.get('RUN_CLANG_TIDY_BIN', 'run-clang-tidy')

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

	# 4. Find files to be processed
	files_to_process = find_source_files(mode)

	if files_to_process is None:
		script_name = os.path.basename(sys.argv[0])
		print(
		    f"usage: {script_name} [c|cpp|both|all] [optional-path-to-.clang-tidy] [--extraargs ...]", file=sys.stderr)
		sys.exit(2)

	# 5. Assemble and execute the final command
	final_command = [run_clang_tidy_bin, *extra_args, *user_extra_args, *files_to_process]

	print(
	    f"[clang-tidy] {run_clang_tidy_bin} {' '.join(extra_args)} {' '.join(user_extra_args)} "
	    f"{len(files_to_process)} files")

	# os.execvp replaces the current process, just like the shell's 'exec'
	try:
		os.execvp(final_command[0], final_command)
	except FileNotFoundError:
		print(f"Error: The command '{final_command[0]}' was not found.", file=sys.stderr)
		print("Please ensure 'run-clang-tidy' is installed and in your PATH.", file=sys.stderr)
		sys.exit(1)
	except OSError as e:
		print(f"Error executing command: {e}", file=sys.stderr)
		sys.exit(1)

if __name__ == "__main__":
	main()
