#!/usr/bin/env python3
"""
Cross-platform build and test launcher.

Defaults to running as a standalone build script.
Special commands trigger main-repo test wrapper behavior.
"""

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

from xflow_shared_functions import (
    Colors, Emoji, is_ci_environment, get_git_root, setup_display, check_command_exists, run_clang_format,
    run_clang_tidy, run_yapf)

# Initialize display settings
setup_display()

def build_project(
        source_dir: Path,
        build_dir: Path,
        rebuild: bool,
        verbose: bool = False,
        build_shared_libs: bool = False,
        build_executable: bool = True,
        debug_build: bool = False):
	"""Build the project using CMake."""
	is_github_actions = os.environ.get("GITHUB_ACTIONS") == "true"
	is_ci = is_github_actions or os.environ.get("CI") is not None

	if rebuild:
		if build_dir.exists():
			print(f"{Emoji.INFO} Removing {build_dir}")
			shutil.rmtree(build_dir)

		cache_dir = Path.home() / ".cache" / "cppcheck"
		if cache_dir.exists():
			shutil.rmtree(cache_dir)

		build_dir.mkdir(parents=True, exist_ok=True)

		nproc = get_nproc()

		if check_command_exists("ninja"):
			generator = ["-G", "Ninja"]
			if verbose:
				build_cmd = ["ninja", "-v", "-j", str(nproc)]
			else:
				build_cmd = ["ninja", "-j", str(nproc)]
		else:
			generator = []
			if verbose:
				build_cmd = ["make", f"-j{nproc}", "VERBOSE=1"]
			else:
				build_cmd = ["make", f"-j{nproc}"]

		if is_github_actions:
			os_name = os.environ.get("RUNNER_OS", platform.system())
		else:
			os_name = platform.system()

		if is_github_actions:
			if os_name == "Windows":
				cc = "C:/deps/llvm-mingw/bin/clang.exe"
				cxx = "C:/deps/llvm-mingw/bin/clang++.exe"
			elif os_name in ["macOS", "Darwin"]:
				cc = "/opt/homebrew/opt/llvm/bin/clang"
				cxx = "/opt/homebrew/opt/llvm/bin/clang++"
			elif os_name == "Linux":
				cc = "/usr/bin/clang"
				cxx = "/usr/bin/clang++"
			else:
				print(f"{Colors.RED}Unsupported OS: {os_name}{Colors.RESET}", file=sys.stderr)
				sys.exit(1)
		else:
			if platform.system() == "Darwin":
				cc = "/opt/homebrew/opt/llvm/bin/clang"
				cxx = "/opt/homebrew/opt/llvm/bin/clang++"
			else:
				cc = "clang"
				cxx = "clang++"

		cmake_verbose = "ON" if verbose else "OFF"

		cmake_prefix_path = ""
		if is_github_actions and os_name == "Windows":
			cmake_prefix_path = "C:/deps/gsl-install;C:/deps/jansson-install;C:/deps/libmodbus"

		# Default to Release for performance, use Debug only when explicitly requested
		build_type = "Debug" if debug_build else "Release"

		cmake_cmd = [
		    "cmake",
		    *generator,
		    "-B",
		    str(build_dir),
		    "-S",
		    str(source_dir),
		    f"-DCMAKE_BUILD_TYPE={build_type}",
		    f"-DCMAKE_VERBOSE_MAKEFILE={cmake_verbose}",
		    f"-DCMAKE_C_COMPILER={cc}",
		    f"-DCMAKE_CXX_COMPILER={cxx}",
		    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
		    f"-DBUILD_XFE_CONTROL_SIM_EXECUTABLE={'ON' if build_executable else 'OFF'}",
		    f"-DBUILD_SHARED_LIBS={'ON' if build_shared_libs else 'OFF'}",
		]

		# Allow overriding config file via environment variable
		config_file_override = os.environ.get("XFE_CONTROL_SIM_CONFIG_FILE")
		if config_file_override:
			cmake_cmd.append(f"-DXFE_CONTROL_SIM_CONFIG_FILE={config_file_override}")
			print(f"{Emoji.INFO} Using config file: {config_file_override}")

		if cmake_prefix_path:
			cmake_cmd.append(f"-DCMAKE_PREFIX_PATH={cmake_prefix_path}")

		print(f"{Emoji.INFO} Configuring with CMake...")
		subprocess.run(cmake_cmd, check=True, cwd=source_dir)

		print(f"{Emoji.INFO} Building with {nproc} processors...")
		subprocess.run(build_cmd, check=True, cwd=build_dir)

def get_nproc() -> int:
	"""Get number of processor cores."""
	system = platform.system()
	if system == "Darwin":
		try:
			result = subprocess.run(
			    ["sysctl", "-n", "hw.ncpu"],
			    capture_output=True,
			    text=True,
			    encoding='utf-8',
			    errors='replace',
			    check=True)
			return int(result.stdout.strip())
		except (subprocess.CalledProcessError, ValueError):
			pass
	return os.cpu_count() or 1

def get_script_dir() -> Path:
	"""Get the directory containing this script."""
	return Path(__file__).parent.resolve()

def run_binary(source_dir: Path, build_dir: Path, binary_name: str) -> int:
	"""Run the compiled binary and return exit code."""
	log_dir = source_dir / "log" / "log_data"
	log_dir.mkdir(parents=True, exist_ok=True)

	bin_dir = build_dir / "executables-out"
	if not bin_dir.exists():
		print(f"{Colors.RED}Binary directory not found: {bin_dir}{Colors.RESET}", file=sys.stderr)
		return 1

	bin_path = bin_dir / binary_name
	if not bin_path.exists():
		bin_path = bin_dir / f"{binary_name}.exe"
		if not bin_path.exists():
			print(f"{Colors.RED}Binary not found: {binary_name}{Colors.RESET}", file=sys.stderr)
			return 1

	print(f"{Emoji.INFO} Running {bin_path}...")
	# Run without capture_output to get real-time streaming output
	result = subprocess.run([str(bin_path)], cwd=bin_dir)
	print()

	return result.returncode

def run_copy_test(repo_root: Path, subdir_name: str, test_type: str, rebuild: bool) -> int:
	"""
    Run test by copying a subdirectory to a temp location and running there.
    
    Used for sim_example_copy_test and sim_example_copy_test_discon.
    """
	run_yapf(repo_root)
	sync_scripts_to_subdir(repo_root, subdir_name)
	run_clang_format(repo_root)

	source_subdir = repo_root / subdir_name
	tmp_root = repo_root.parent / f"{subdir_name}_test"

	if test_type == "discon":
		test_name = "DISCON"
		command_arg = "discon"
	else:
		test_name = subdir_name
		command_arg = "xfe_control_sim"

	print(f"{Emoji.INFO} Testing {test_name} in temporary dir: {tmp_root}")

	if rebuild:
		if tmp_root.exists():
			shutil.rmtree(tmp_root)

	if not tmp_root.exists():
		print(f"{Emoji.INFO} Copying {subdir_name} to {tmp_root}")
		shutil.copytree(source_subdir, tmp_root)

	print(f"{Emoji.INFO} Building + running via {tmp_root}/misc/launch_tests.py")

	launcher_path = tmp_root / "misc" / "launch_tests.py"
	if not launcher_path.exists():
		print(f"{Colors.RED}Launcher script not found: {launcher_path}{Colors.RESET}", file=sys.stderr)
		print(
		    f"{Colors.YELLOW}   Make sure launch_tests.py is copied to {subdir_name}/misc/{Colors.RESET}",
		    file=sys.stderr)
		return 1

	result = subprocess.run(
	    [sys.executable, str(launcher_path), command_arg, "1" if rebuild else "0"], cwd=tmp_root / "misc")
	launch_exit = result.returncode

	if launch_exit != 0:
		print(f"{Colors.RED}{test_name} test run failed (exit {launch_exit}){Colors.RESET}", file=sys.stderr)
	else:
		print(f"{Colors.GREEN}{test_name} test run succeeded{Colors.RESET}")

	log_file = tmp_root / "log" / "log_data" / "xfe-control-sim-simulation-output.log"
	log_ok = validate_log_file(log_file, test_type)

	os.chdir(repo_root)

	if log_ok:
		print(f"{Colors.GREEN}Log validation passed. Cleaning up temp folder: {tmp_root}{Colors.RESET}")
		shutil.rmtree(tmp_root)
		return launch_exit
	else:
		print(
		    f"{Colors.RED}Log validation failed. Preserving temp folder for inspection: {tmp_root}{Colors.RESET}",
		    file=sys.stderr)
		print(f"{Colors.YELLOW}   You can inspect the log with: less '{log_file}'{Colors.RESET}", file=sys.stderr)
		return launch_exit if launch_exit != 0 else 1

def run_all_tests(repo_root: Path, rebuild: bool, verbose: bool = False, debug_build: bool = False) -> int:
	"""
    Build and run all test executables without running the main simulation.
    """
	run_yapf(repo_root)
	sync_scripts_to_subdir(repo_root, "sim_example")
	run_clang_format(repo_root)

	build_dir = repo_root / "build"

	print(f"{Emoji.INFO} Building test executables from main repo...")
	build_project(
	    repo_root, build_dir, rebuild, verbose, build_shared_libs=False, build_executable=True, debug_build=debug_build)

	print()

	# List of all test executables.
	# test_numerical_integrator is split into one binary per test function so
	# that stateful integrators (AB2) start fresh in each process. See
	# tests/CMakeLists.txt.
	test_executables = [
	    "test_flow_gen",
	    "test_param_array",
	    "test_common_utils",
	    "test_control_switch",
	    # Numerical integrator: per-test binaries (process isolation)
	    "test_euler_integrator_exists",
	    "test_rk4_integrator_exists",
	    "test_ab2_integrator_exists",
	    "test_numerical_integrator_map_has_entries",
	    "test_integrator_map_contains_expected_functions",
	    "test_integrators_handle_zero_timestep",
	    "test_integrators_handle_multiple_state_vars",
	    "test_integrators_handle_single_state_var",
	    "test_integrators_handle_negative_timestep",
	    "test_integrators_repeated_calls",
	    "test_euler_vs_rk4_different_results",
	    "test_euler_accuracy_decay",
	    "test_rk4_accuracy_decay",
	    "test_ab2_accuracy_decay",
	    "test_integrator_accuracy_ordering",
	    "test_euler_convergence_order",
	]

	bin_dir = build_dir / "executables-out"
	failed_tests = []
	passed_tests = []
	missing_tests = []

	for test_name in test_executables:
		test_binary = bin_dir / test_name
		if not test_binary.exists():
			test_binary = bin_dir / f"{test_name}.exe"
			if not test_binary.exists():
				print(f"{Colors.YELLOW}Test binary not found: {test_name}{Colors.RESET}")
				missing_tests.append(test_name)
				continue

		print(f"\n{Emoji.INFO} Running {test_name}...")
		print("=" * 70)
		test_result = subprocess.run([str(test_binary)], capture_output=False)
		print("=" * 70)

		if test_result.returncode != 0:
			print(f"{Colors.RED}{test_name} FAILED{Colors.RESET}")
			failed_tests.append(test_name)
		else:
			print(f"{Colors.GREEN}{test_name} PASSED{Colors.RESET}")
			passed_tests.append(test_name)

	# Print summary
	print(f"\n{Colors.BLUE}{'=' * 70}{Colors.RESET}")
	print(f"{Colors.BLUE}Test Summary{Colors.RESET}")
	print(f"{Colors.BLUE}{'=' * 70}{Colors.RESET}")
	print(f"Total tests:   {len(test_executables)}")
	print(f"{Colors.GREEN}Passed:        {len(passed_tests)}{Colors.RESET}")
	print(f"{Colors.RED}Failed:        {len(failed_tests)}{Colors.RESET}")
	print(f"{Colors.YELLOW}Missing:       {len(missing_tests)}{Colors.RESET}")

	if failed_tests:
		print(f"\n{Colors.RED}Failed tests:{Colors.RESET}")
		for test in failed_tests:
			print(f"  - {test}")

	if missing_tests:
		print(f"\n{Colors.YELLOW}Missing tests:{Colors.RESET}")
		for test in missing_tests:
			print(f"  - {test}")

	print(f"{Colors.BLUE}{'=' * 70}{Colors.RESET}\n")

	return 1 if failed_tests else 0

def run_main_repo_build(repo_root: Path, rebuild: bool, verbose: bool = False, debug_build: bool = False) -> int:
	"""
    Run build from the main xfe-control-sim repo (local_xfe_control_sim command).
    """
	run_yapf(repo_root)
	sync_scripts_to_subdir(repo_root, "sim_example")
	run_clang_format(repo_root)

	build_dir = repo_root / "build"

	print(f"{Emoji.INFO} Building xfe_control_sim from main repo...")
	build_project(
	    repo_root, build_dir, rebuild, verbose, build_shared_libs=False, build_executable=True, debug_build=debug_build)

	run_clang_tidy_enabled = os.environ.get("RUN_CLANG_TIDY", "1")
	is_ci = is_ci_environment()

	if is_ci or run_clang_tidy_enabled == "1":
		run_clang_tidy(repo_root, build_dir)
	else:
		print(f"{Colors.YELLOW}[INFO] RUN_CLANG_TIDY not set; skipping clang-tidy step.{Colors.RESET}")

	print()

	exit_code = run_binary(repo_root, build_dir, "xfe_control_sim")

	log_file = repo_root / "log" / "log_data" / "xfe-control-sim-simulation-output.log"
	if log_file.exists():
		print(f"{Emoji.INFO} Contents of simulation log:")
		with open(log_file, "r", encoding='utf-8', errors='replace') as f:
			print(f.read())
		print()
	else:
		print(f"{Colors.YELLOW}Log file not found: {log_file}{Colors.RESET}")

	log_ok = validate_log_file(log_file, "xfe_control_sim")
	if log_ok:
		print(f"{Colors.GREEN}Log validation passed{Colors.RESET}")
	if not log_ok and exit_code == 0:
		exit_code = 1

	return exit_code

def run_standalone_build(
        build_dir_name: str, test_type: str, rebuild: bool, verbose: bool = False, debug_build: bool = False) -> int:
	"""
    Default mode: Run as a standalone build script.

    This is what runs when the script is in a copied directory like sim_example.
    """
	script_dir = get_script_dir()
	source_dir = script_dir.parent
	is_ci = is_ci_environment()

	repo_root = get_git_root(False)
	if repo_root is None:
		repo_root = source_dir
		print(f"{Colors.YELLOW}[INFO] Not in a git repository - using local directory structure{Colors.RESET}")

	if not is_ci:
		run_clang_format(repo_root)

	build_dir = source_dir / build_dir_name

	if test_type == "discon":
		print(f"{Emoji.INFO} Building DISCON interface test...")
		binary_name = "qblade_interface_test"
		build_shared_libs = True
		build_executable = False
	else:
		print(f"{Emoji.INFO} Building xfe_control_sim...")
		binary_name = "xfe_control_sim"
		build_shared_libs = False
		build_executable = True

	build_project(source_dir, build_dir, rebuild, verbose, build_shared_libs, build_executable, debug_build)

	run_clang_tidy_enabled = os.environ.get("RUN_CLANG_TIDY", "1")

	if is_ci or run_clang_tidy_enabled == "1":
		run_clang_tidy(repo_root, build_dir)
	else:
		print(f"{Colors.YELLOW}[INFO] RUN_CLANG_TIDY not set; skipping clang-tidy step.{Colors.RESET}")

	print()

	exit_code = run_binary(source_dir, build_dir, binary_name)

	if test_type == "discon":
		if exit_code != 0:
			print(f"{Colors.RED}qblade interface test failed (exit {exit_code}){Colors.RESET}", file=sys.stderr)
		else:
			print(f"{Colors.GREEN}qblade interface test passed!{Colors.RESET}")

	log_file = source_dir / "log" / "log_data" / "xfe-control-sim-simulation-output.log"
	if log_file.exists():
		print(f"{Emoji.INFO} Contents of simulation log:")
		with open(log_file, "r", encoding='utf-8', errors='replace') as f:
			print(f.read())
		print()
	else:
		print(f"{Colors.YELLOW}Log file not found: {log_file}{Colors.RESET}")

	if test_type == "xfe_control_sim":
		log_ok = validate_log_file(log_file, "xfe_control_sim")
		if log_ok:
			print(f"{Colors.GREEN}Log validation passed{Colors.RESET}")
		if not log_ok and exit_code == 0:
			exit_code = 1

	return exit_code

def sync_scripts_to_subdir(repo_root: Path, subdir_name: str):
	"""
    Copy scripts and config files from main repo to subdirectory.
    
    This ensures the subdirectory has the latest versions.
    """
	is_github_actions = os.environ.get("GITHUB_ACTIONS") == "true"
	is_ci = is_github_actions or os.environ.get("CI") is not None

	if is_ci:
		print(f"{Colors.YELLOW}Not syncing scripts since we are in CI{Colors.RESET}")
		return

	source_misc = repo_root / "misc"
	dest_misc = repo_root / subdir_name / "misc"
	dest_root = repo_root / subdir_name

	if not dest_misc.exists():
		print(f"{Colors.YELLOW}[WARN] Destination misc directory not found: {dest_misc}{Colors.RESET}")
		return

	print(f"{Emoji.INFO} Syncing scripts to {subdir_name}/misc/...")

	# Copy Python scripts to misc/
	scripts_to_copy = ["launch_tests.py", "clang_format_all.py", "clang_tidy_all.py", "xflow_shared_functions.py"]
	for script_name in scripts_to_copy:
		source_file = source_misc / script_name
		dest_file = dest_misc / script_name

		if source_file.exists():
			shutil.copy2(source_file, dest_file)
			print(f"   {Colors.GREEN}Copied {script_name}{Colors.RESET}")
		else:
			print(f"   {Colors.YELLOW}Skipped {script_name} (not found){Colors.RESET}")

	# Copy config files to subdirectory root
	config_files = {
	    ".clang-format": ["c/src", ".", "misc"],
	    ".clang-tidy": ["c/src", ".", "misc"],
	    ".style.yapf": [".", "misc"]
	}

	print(f"{Emoji.INFO} Syncing config files to {subdir_name}/...")
	for config_name, search_paths in config_files.items():
		found = False
		for search_path in search_paths:
			source_file = repo_root / search_path / config_name
			if source_file.exists():
				dest_file = dest_root / config_name
				shutil.copy2(source_file, dest_file)
				print(f"   {Colors.GREEN}Copied {config_name} from {search_path}/{Colors.RESET}")
				found = True
				break

		if not found:
			print(f"   {Colors.YELLOW}Skipped {config_name} (not found){Colors.RESET}")

	print()

def validate_log_file(log_file: Path, test_type: str) -> bool:
	"""Validate the log file contents."""
	if not log_file.exists():
		print(f"{Colors.RED}Log file not found: {log_file}{Colors.RESET}", file=sys.stderr)
		return False

	print(f"{Emoji.INFO} Validating log file: {log_file}")

	with open(log_file, "r", encoding='utf-8', errors='replace') as f:
		content = f.read()
		lines = [line for line in content.split("\n") if line.strip()]

	log_ok = True

	if test_type == "discon":
		if "discon init complete!" not in content:
			print(f"{Colors.RED}Missing 'discon init complete!' line.{Colors.RESET}", file=sys.stderr)
			log_ok = False
	else:  # xfe_control_sim
		if "Program Duration:" not in content:
			print(f"{Colors.RED}Missing 'Program Duration:' line.{Colors.RESET}", file=sys.stderr)
			log_ok = False

		if "write Duration:" not in content:
			print(f"{Colors.RED}Missing 'write Duration:' line.{Colors.RESET}", file=sys.stderr)
			log_ok = False

		if lines:
			last_line = lines[-1]
			if "Closing Program" not in last_line:
				print(f"{Colors.RED}Last non-empty line is not 'Closing Program'.{Colors.RESET}", file=sys.stderr)
				print(f"{Colors.YELLOW}   Last line was: {last_line}{Colors.RESET}", file=sys.stderr)
				log_ok = False

	if "ERROR" in content:
		print(f"{Colors.RED}Found error lines in log:{Colors.RESET}", file=sys.stderr)
		for line in content.split("\n"):
			if "ERROR" in line:
				print(f"{Colors.YELLOW}   {line}{Colors.RESET}", file=sys.stderr)
		log_ok = False

	return log_ok

def main():
	"""Main entry point."""
	parser = argparse.ArgumentParser(
	    description="Cross-platform build and test launcher (defaults to Release build for performance)",
	    epilog="""
Standalone build commands (default - run from any build directory):
  %(prog)s xfe_control_sim 1         Build and run xfe_control_sim (Release mode, with rebuild)
  %(prog)s discon 0                  Build and run DISCON test (no rebuild)
  %(prog)s xfe_control_sim 0 -v      Build xfe_control_sim (no rebuild, verbose)
  %(prog)s xfe_control_sim 1 --debug Build with Debug mode (slower, includes debug symbols)

Main repo commands (run from xfe-control-sim/misc/):
  %(prog)s local_xfe_control_sim 1            Build and run in main repo (Release mode, with rebuild)
  %(prog)s local_xfe_control_sim 1 --debug    Build in Debug mode (slower, for debugging)
  %(prog)s run_tests 1                        Build and run ALL tests (with rebuild)
  %(prog)s run_tests 0                        Run ALL tests (no rebuild)
  %(prog)s sim_example_copy_test 1            Copy sim_example to temp, build, and test
  %(prog)s sim_example_copy_test_discon 0     Copy sim_example to temp and test DISCON
        """,
	    formatter_class=argparse.RawDescriptionHelpFormatter)

	parser.add_argument("command", nargs="?", help="Which build/test to run")
	parser.add_argument(
	    "rebuild",
	    nargs="?",
	    default="0",
	    choices=["0", "1"],
	    help="Whether to rebuild (1) or reuse existing build (0)")
	parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose build output")
	parser.add_argument(
	    "--debug",
	    action="store_true",
	    help="Build with Debug mode instead of Release (slower, includes debug symbols)")
	parser.add_argument("--build-dir", default="build", help="Build directory name (default: build)")
	parser.add_argument(
	    "--subdir", default="sim_example", help="Subdirectory name for copy tests (default: sim_example)")

	args = parser.parse_args()

	if args.command is None:
		parser.print_help()
		sys.exit(0)

	rebuild = args.rebuild == "1"

	if args.command == "local_xfe_control_sim":
		repo_root = get_git_root()
		if repo_root is None:
			print(
			    f"{Colors.RED}Error: local_xfe_control_sim requires being in a git repository{Colors.RESET}",
			    file=sys.stderr)
			sys.exit(1)
		exit_code = run_main_repo_build(repo_root, rebuild, args.verbose, args.debug)

	elif args.command == "run_tests":
		repo_root = get_git_root()
		if repo_root is None:
			print(f"{Colors.RED}Error: run_tests requires being in a git repository{Colors.RESET}", file=sys.stderr)
			sys.exit(1)
		exit_code = run_all_tests(repo_root, rebuild, args.verbose, args.debug)

	elif args.command == "sim_example_copy_test":
		repo_root = get_git_root()
		if repo_root is None:
			print(
			    f"{Colors.RED}Error: sim_example_copy_test requires being in a git repository{Colors.RESET}",
			    file=sys.stderr)
			sys.exit(1)
		exit_code = run_copy_test(repo_root, args.subdir, "xfe_control_sim", rebuild)

	elif args.command == "sim_example_copy_test_discon":
		repo_root = get_git_root()
		if repo_root is None:
			print(
			    f"{Colors.RED}Error: sim_example_copy_test_discon requires being in a git repository{Colors.RESET}",
			    file=sys.stderr)
			sys.exit(1)
		exit_code = run_copy_test(repo_root, args.subdir, "discon", rebuild)

	elif args.command == "xfe_control_sim":
		exit_code = run_standalone_build(args.build_dir, "xfe_control_sim", rebuild, args.verbose, args.debug)

	elif args.command == "discon":
		exit_code = run_standalone_build(args.build_dir, "discon", rebuild, args.verbose, args.debug)

	else:
		print(f"{Colors.RED}Unknown command: {args.command}{Colors.RESET}", file=sys.stderr)
		print(f"\n{Colors.YELLOW}Standalone commands: xfe_control_sim, discon{Colors.RESET}", file=sys.stderr)
		print(
		    f"{Colors.YELLOW}Main repo commands: local_xfe_control_sim, run_tests, sim_example_copy_test, "
		    f"sim_example_copy_test_discon{Colors.RESET}",
		    file=sys.stderr)
		sys.exit(1)

	sys.exit(exit_code)

if __name__ == "__main__":
	main()
