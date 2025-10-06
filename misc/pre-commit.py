#!/usr/bin/env python3
"""
Pre-commit hook for code formatting.

This script runs clang-format in pre-commit mode, which will:
1. Check if any files need formatting
2. If yes, format them AND abort the commit
3. If no, allow the commit to proceed

This ensures code is always formatted, but gives the developer
a chance to review the changes before they're committed.
"""

import os
import subprocess
import sys
from pathlib import Path

# Find git root and locate the shared module dynamically
try:
	result = subprocess.run(
	    ['git', 'rev-parse', '--show-toplevel'],
	    capture_output=True,
	    text=True,
	    encoding='utf-8',
	    errors='replace',
	    check=True)
	repo_root = Path(result.stdout.strip())

	# Search for xflow_shared_functions.py in common locations
	search_paths = [
	    repo_root / 'misc',
	    repo_root / 'scripts',
	    repo_root / 'tools',
	    repo_root / 'c' / 'src' / 'misc',
	    repo_root / 'src' / 'misc',
	]

	# Also check environment variable
	if 'XFLOW_SHARED_FUNCTIONS_DIR' in os.environ:
		search_paths.insert(0, Path(os.environ['XFLOW_SHARED_FUNCTIONS_DIR']))

	shared_module_dir = None
	for path in search_paths:
		if (path / 'xflow_shared_functions.py').exists():
			shared_module_dir = path
			break

	if shared_module_dir:
		sys.path.insert(0, str(shared_module_dir))
	else:
		print("ERROR: Could not find xflow_shared_functions.py", file=sys.stderr)
		print("Searched in:", file=sys.stderr)
		for path in search_paths:
			print(f"  - {path}", file=sys.stderr)
		sys.exit(1)

except (subprocess.CalledProcessError, FileNotFoundError):
	print("ERROR: Not in a git repository", file=sys.stderr)
	sys.exit(1)

# Now we can import from wherever we found it
from xflow_shared_functions import Colors, setup_display, find_file

# Initialize display settings
setup_display()

def main():
	"""Main entry point for pre-commit hook."""
	python_executable = sys.executable

	try:
		result = subprocess.run(
		    ['git', 'rev-parse', '--show-toplevel'],
		    capture_output=True,
		    text=True,
		    encoding='utf-8',
		    errors='replace',
		    check=True)
		git_root = result.stdout.strip()
	except subprocess.CalledProcessError:
		print(f"\n{Colors.RED}ERROR: Not in a git repository. Aborting.{Colors.RESET}", file=sys.stderr)
		sys.exit(1)
	except FileNotFoundError:
		print(f"\n{Colors.RED}ERROR: Git not found. Aborting.{Colors.RESET}", file=sys.stderr)
		sys.exit(1)

	script_to_run = find_file(
	    project_root=git_root, script_name="clang_format_all.py", env_var_name="CLANG_FORMAT_SCRIPT", max_depth=4)

	if not script_to_run:
		print(f"\n{Colors.RED}ERROR: Could not find clang_format_all.py in repository.{Colors.RESET}", file=sys.stderr)
		print(f"{Colors.YELLOW}Searched in:{Colors.RESET}", file=sys.stderr)
		print(f"  - Repository root", file=sys.stderr)
		print(f"  - scripts/, tools/, src/, misc/, c/src/misc/", file=sys.stderr)
		print(
		    f"\n{Colors.YELLOW}Tip: Set CLANG_FORMAT_SCRIPT environment variable "
		    f"to specify location{Colors.RESET}",
		    file=sys.stderr)
		sys.exit(1)

	print(f"{Colors.BOLD}--- Running pre-commit format check ---{Colors.RESET}")
	print(f"Repository: {Colors.BLUE}{git_root}{Colors.RESET}")
	print(f"Script: {Colors.BLUE}{os.path.relpath(script_to_run, git_root)}{Colors.RESET}")
	print()

	try:
		result = subprocess.run([python_executable, script_to_run, 'pre-commit'], capture_output=False, cwd=git_root)
		sys.exit(result.returncode)

	except FileNotFoundError as e:
		print(f"\n{Colors.RED}ERROR: Could not execute script: {e}{Colors.RESET}", file=sys.stderr)
		sys.exit(1)
	except KeyboardInterrupt:
		print(f"\n{Colors.YELLOW}Interrupted by user. Aborting commit.{Colors.RESET}", file=sys.stderr)
		sys.exit(1)

if __name__ == '__main__':
	main()
