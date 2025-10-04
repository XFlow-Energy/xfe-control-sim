#!/usr/bin/env python3
"""
A cross-platform Python script to run clang-format on C/C++ source files.

This script replicates the functionality of the provided shell script:
1. Finds the top-level directory of the git repository.
2. Determines clang-format flags based on command-line arguments ('dry-run' or default).
3. Traverses the repository, finding all files with .c, .h, .cpp, and .hpp extensions.
4. Excludes the 'build' directory from the search.
5. Executes clang-format on all found files.
"""

import os
import sys
import subprocess

def find_git_toplevel():
	"""Finds the root directory of the git repository."""
	try:
		# Use subprocess to run the git command
		result = subprocess.run(
		    ['git', 'rev-parse', '--show-toplevel'], capture_output=True, text=True, check=True, encoding='utf-8')
		# Strip any trailing whitespace/newlines from the output
		return result.stdout.strip()
	except (subprocess.CalledProcessError, FileNotFoundError):
		# Handle cases where git isn't installed or this isn't a git repo
		print(
		    "ERROR: Could not find git repository root. "
		    "Make sure you are in a git repository and 'git' is in your PATH.",
		    file=sys.stderr)
		return None

def main():
	"""Main function to execute the script logic."""
	# 1. Get the top-level directory of the git repo
	toplevel = find_git_toplevel()
	if not toplevel:
		sys.exit(1)

	print(f"Descending from git root: {toplevel}")

	# 2. Choose flags based on CI mode (command-line arguments)
	if len(sys.argv) > 1 and sys.argv[1] == 'dry-run':
		# In dry-run mode, check for changes and report errors without modifying files.
		flags = ['--verbose', '--dry-run', '-Werror']
		print("Running in 'dry-run' mode.")
	else:
		# Default mode: format files in-place.
		flags = ['--verbose', '-i']
		print("Running in in-place formatting mode (-i).")

	# 3. Find all target source files
	source_files = []
	extensions = ('.c', '.h', '.cpp', '.hpp')
	build_dir_name = 'build'

	# Walk through the directory tree starting from the top level
	for root, dirs, files in os.walk(toplevel):
		# Prune the 'build' directory to avoid formatting generated files.
		# This is equivalent to `find ... -path "*/build" -prune`
		if build_dir_name in dirs:
			dirs.remove(build_dir_name)
			print(f"--> Skipping directory: {os.path.join(root, build_dir_name)}")

		for filename in files:
			# Check if the file has one of the desired extensions
			if filename.endswith(extensions):
				source_files.append(os.path.join(root, filename))

	if not source_files:
		print("No source files found to format.")
		sys.exit(0)

	print(f"Found {len(source_files)} source files to process.")

	# 4. Execute clang-format on the collected files
	style_option = f"--style=file:{os.path.join(toplevel, '.clang-format')}"
	command = ['clang-format'] + flags + [style_option] + source_files

	print("\nExecuting clang-format...")
	# For verbosity, similar to `set -x`, we can print the command.
	# On Windows, a very long command might fail, but this is rare.
	# print(f"Command: {' '.join(command)}") # Uncomment for debugging

	try:
		# Run the command. `check=True` will raise an exception on a non-zero exit code.
		subprocess.run(command, check=True)
		print("\nClang-format run completed successfully.")
	except FileNotFoundError:
		print(
		    "ERROR: 'clang-format' command not found. "
		    "Please ensure it is installed and in your system's PATH.",
		    file=sys.stderr)
		sys.exit(1)
	except subprocess.CalledProcessError as e:
		print(
		    f"\nERROR: clang-format exited with error code {e.returncode}. "
		    "Run with 'dry-run' argument to see formatting differences.",
		    file=sys.stderr)
		sys.exit(e.returncode)

if __name__ == "__main__":
	main()
