"""
A cross-platform Python script to run clang-tidy.

This script determines the project root, build directory, and clang-tidy binary
from environment variables or sensible defaults. It parses command-line arguments
to set the analysis mode (c, cpp, all), specify a .clang-tidy file, and pass
extra arguments to the clang-tidy runner.
"""

import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List, Optional, Set

from xflow_shared_functions import Colors, Emoji, setup_display, find_file, find_run_clang_tidy

# Initialize display settings
setup_display()

def find_source_files(
        mode: str,
        project_root: str,
        build_dir: str,
        exclude_dirs: Optional[Set[str]] = None,
        verbose: bool = False) -> Optional[List[str]]:
	"""
    Find source files in the project based on the mode.

    Args:
        mode: File selection mode ('c', 'cpp', 'all', or 'both').
        project_root: Root directory to search from.
        build_dir: Build directory to exclude.
        exclude_dirs: Additional directories to exclude.
        verbose: Print detailed progress.

    Returns:
        List of file paths matching the mode, or None if mode is invalid.
    """
	c_ext = ('.c',)
	cpp_ext = ('.cc', '.cpp', '.cxx')
	all_ext = c_ext + cpp_ext

	extensions = {'c': c_ext, 'cpp': cpp_ext, 'all': all_ext, 'both': all_ext}

	target_extensions = extensions.get(mode)
	if not target_extensions:
		print(f"{Colors.RED}Error: Invalid mode '{mode}'{Colors.RESET}", file=sys.stderr)
		print(f"Valid modes: c, cpp, all, both", file=sys.stderr)
		return None

	if exclude_dirs is None:
		exclude_dirs = {'.git', 'third_party', 'external', 'vendor', '.venv', 'venv'}

	if 'CLANG_TIDY_EXCLUDE' in os.environ:
		exclude_dirs.update(os.environ['CLANG_TIDY_EXCLUDE'].split(','))

	build_dir_name = os.path.basename(build_dir)
	exclude_dirs.add(build_dir_name)

	if verbose:
		print(f"Searching for {mode} files in: {project_root}")
		print(f"Excluding directories: {', '.join(sorted(exclude_dirs))}")

	found_files = []
	for root, dirs, files in os.walk(project_root):
		dirs[:] = [d for d in dirs if d not in exclude_dirs]

		for f in files:
			if f.endswith(target_extensions):
				relative_path = os.path.relpath(os.path.join(root, f), start=project_root)
				found_files.append(relative_path)
				if verbose:
					print(f"  Found: {relative_path}")

	return sorted(found_files)

def get_macos_sdk_path(verbose: bool = False) -> Optional[str]:
	"""
    Get macOS SDK path using xcrun.
    
    Returns:
        SDK path or None if not available.
    """
	if platform.system() != 'Darwin':
		return None

	try:
		result = subprocess.run(
		    ['xcrun', '--show-sdk-path'],
		    capture_output=True,
		    text=True,
		    encoding='utf-8',
		    errors='replace',
		    check=True)
		sdk_path = result.stdout.strip()
		if sdk_path:
			if verbose:
				print(f"macOS SDK path: {sdk_path}")
			return sdk_path
	except (FileNotFoundError, subprocess.CalledProcessError) as e:
		if verbose:
			print(f"{Colors.YELLOW}Warning: Could not get macOS SDK path: {e}{Colors.RESET}", file=sys.stderr)

	return None

def main():
	"""Main execution function."""
	args = sys.argv[1:]
	verbose = False
	dry_run = False

	if '-h' in args or '--help' in args:
		print(
		    f"""
{Colors.BOLD}Usage:{Colors.RESET} {sys.argv[0]} [mode] [config-file] [--extraargs args...] [options]

{Colors.BOLD}Modes:{Colors.RESET}
  c           Analyze C files only (.c)
  cpp         Analyze C++ files only (.cc, .cpp, .cxx)
  all         Analyze all C and C++ files (default)
  both        Same as 'all'

{Colors.BOLD}Arguments:{Colors.RESET}
  config-file     Path to .clang-tidy config file (optional)
  --extraargs     Pass additional arguments to run-clang-tidy
                  (must be last argument before extra args)

{Colors.BOLD}Options:{Colors.RESET}
  -v, --verbose   Show detailed progress
  --dry-run       Show what would be analyzed without running clang-tidy
  -h, --help      Show this help message

{Colors.BOLD}Environment Variables:{Colors.RESET}
  PROJECT_ROOT           Project root directory (default: current directory)
  BUILD_DIR              Build directory with compile_commands.json
  RUN_CLANG_TIDY_BIN     Path to run-clang-tidy executable
  CLANG_TIDY_CONFIG      Path to .clang-tidy configuration file
  CLANG_TIDY_EXCLUDE     Comma-separated list of directories to exclude

{Colors.BOLD}Examples:{Colors.RESET}
  {sys.argv[0]} all
  {sys.argv[0]} cpp
  {sys.argv[0]} all /path/to/.clang-tidy
  {sys.argv[0]} all --extraargs -fix
  BUILD_DIR=build-debug {sys.argv[0]} all
        """)
		sys.exit(0)

	if '-v' in args or '--verbose' in args:
		verbose = True
		args = [a for a in args if a not in ['-v', '--verbose']]

	if '--dry-run' in args:
		dry_run = True
		args = [a for a in args if a != '--dry-run']

	project_root = os.environ.get('PROJECT_ROOT', os.getcwd())
	build_dir = os.environ.get('BUILD_DIR', os.path.join(project_root, 'build'))

	if not os.path.exists(project_root):
		print(f"{Colors.RED}Error: Project root does not exist: {project_root}{Colors.RESET}", file=sys.stderr)
		sys.exit(1)

	if not os.path.exists(build_dir):
		print(f"{Colors.RED}Error: Build directory does not exist: {build_dir}{Colors.RESET}", file=sys.stderr)
		print(f"Create it by running CMake with: -DCMAKE_EXPORT_COMPILE_COMMANDS=ON", file=sys.stderr)
		sys.exit(1)

	compile_commands = os.path.join(build_dir, 'compile_commands.json')
	if not os.path.exists(compile_commands):
		print(f"{Colors.YELLOW}Warning: compile_commands.json not found in: {build_dir}{Colors.RESET}", file=sys.stderr)
		print(f"Make sure to configure CMake with: -DCMAKE_EXPORT_COMPILE_COMMANDS=ON", file=sys.stderr)

	run_clang_tidy_bin = find_run_clang_tidy()
	if not run_clang_tidy_bin:
		print(f"{Colors.RED}Error: run-clang-tidy not found{Colors.RESET}", file=sys.stderr)
		print("Install clang-tidy or set RUN_CLANG_TIDY_BIN environment variable", file=sys.stderr)
		sys.exit(1)

	if verbose:
		print(f"Project root: {Colors.BLUE}{project_root}{Colors.RESET}")
		print(f"Build directory: {Colors.BLUE}{build_dir}{Colors.RESET}")
		print(f"Using run-clang-tidy: {Colors.BLUE}{run_clang_tidy_bin}{Colors.RESET}")

	mode = 'all'
	if args and args[0] in ['c', 'cpp', 'both', 'all']:
		mode = args.pop(0)

	clang_tidy_file = None
	if args and args[0] != '--extraargs':
		clang_tidy_file = args.pop(0)
		if not os.path.exists(clang_tidy_file):
			print(f"{Colors.RED}Error: Config file not found: {clang_tidy_file}{Colors.RESET}", file=sys.stderr)
			sys.exit(1)
	else:
		clang_tidy_file = find_file(
		    project_root=project_root, script_name=".clang-tidy", env_var_name="CLANG_TIDY_CONFIG", max_depth=3)

	user_extra_args = []
	if args and args[0] == '--extraargs':
		args.pop(0)
		user_extra_args = args

	os.chdir(project_root)

	files_to_process = find_source_files(mode, project_root, build_dir, verbose=verbose)

	if files_to_process is None:
		sys.exit(2)

	if not files_to_process:
		print(f"{Colors.YELLOW}No {mode} files found to analyze{Colors.RESET}")
		sys.exit(0)

	print(f"Found {Colors.BOLD}{len(files_to_process)}{Colors.RESET} {mode} file(s) to analyze")

	files_to_process = [os.path.abspath(p) for p in files_to_process]

	if platform.system() == "Windows":
		files_to_process = [p.replace("\\", "\\\\") for p in files_to_process]

	if verbose and files_to_process:
		print(f"First file: {files_to_process[0]}")

	try:
		nproc = os.cpu_count() or 1
	except NotImplementedError:
		nproc = 1

	extra_args = [f'-j{nproc}', f'-p={build_dir}']

	if verbose:
		print(f"Using {nproc} parallel jobs")

	sdk_path = get_macos_sdk_path(verbose)
	if sdk_path:
		extra_args.append(f'-extra-arg=-isysroot{sdk_path}')

	if clang_tidy_file:
		if os.path.isfile(str(clang_tidy_file)):
			print(f"Using config file: {Colors.GREEN}{clang_tidy_file}{Colors.RESET}")
			extra_args.append(f'-config-file={clang_tidy_file}')
		else:
			print(f"{Colors.YELLOW}Warning: Config file not found: {clang_tidy_file}{Colors.RESET}", file=sys.stderr)
	else:
		print(f"{Colors.YELLOW}Warning: No .clang-tidy config file found{Colors.RESET}", file=sys.stderr)
		print(
		    f"Tip: Set CLANG_TIDY_CONFIG environment variable or create "
		    f".clang-tidy in project root",
		    file=sys.stderr)

	# In clang_tidy_all.py, around line where final_command is built:
	if platform.system() == "Windows":
		# On Windows, if run-clang-tidy has no extension, run it with Python
		run_clang_tidy_bin = os.environ.get('RUN_CLANG_TIDY_BIN', 'run-clang-tidy')
		if not run_clang_tidy_bin.endswith(('.py', '.exe')):
			final_command = [sys.executable, run_clang_tidy_bin, *extra_args, *user_extra_args, *files_to_process]
		else:
			final_command = [run_clang_tidy_bin, *extra_args, *user_extra_args, *files_to_process]
	else:
		final_command = [run_clang_tidy_bin, *extra_args, *user_extra_args, *files_to_process]

	if dry_run:
		print(f"\n{Colors.BOLD}Dry-run mode - command that would be executed:{Colors.RESET}")
		print(f"{Colors.BLUE}{' '.join(final_command[:5])} ...{Colors.RESET}")
		print(f"\n{Colors.BOLD}Files to analyze:{Colors.RESET}")
		for f in sorted(files_to_process[:20]):
			display_path = f.replace("\\\\", "\\") if platform.system() == "Windows" else f
			print(f"  {display_path}")
		if len(files_to_process) > 20:
			print(f"  ... and {len(files_to_process) - 20} more files")
		sys.exit(0)

	print(f"\n{Colors.BOLD}Running clang-tidy...{Colors.RESET}")
	if verbose:
		print(f"Command: {' '.join(final_command[:5])} ... ({len(files_to_process)} files)")

	try:
		subprocess.run(final_command, check=True)
		print(f"\n{Colors.GREEN}{Emoji.CHECK} Analysis complete{Colors.RESET}")
	except subprocess.CalledProcessError as e:
		print(
		    f"\n{Colors.RED}{Emoji.CROSS} clang-tidy failed with exit code "
		    f"{e.returncode}{Colors.RESET}",
		    file=sys.stderr)
		sys.exit(e.returncode)
	except OSError as e:
		print(f"{Colors.RED}Error executing command: {e}{Colors.RESET}", file=sys.stderr)
		sys.exit(1)

if __name__ == "__main__":
	main()
