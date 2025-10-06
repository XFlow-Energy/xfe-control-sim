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
        build_executable: bool = True):
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

		build_type = "Release" if is_ci else "Debug"
		if not build_executable:
			build_type = "Release"
		else:
			build_type = "Debug"

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
			result = subprocess.run(["sysctl", "-n", "hw.ncpu"], capture_output=True, text=True, check=True)
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
	result = subprocess.run(
	    [str(bin_path)], cwd=bin_dir, capture_output=True, text=True, encoding='utf-8', errors='replace')

	print(result.stdout)
	if result.stderr:
		print(result.stderr, file=sys.stderr)
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

def run_main_repo_build(repo_root: Path, rebuild: bool, verbose: bool = False) -> int:
	"""
    Run build from the main xfe-control-sim repo (local_xfe_control_sim command).
    """
	run_yapf(repo_root)
	sync_scripts_to_subdir(repo_root, "sim_example")
	run_clang_format(repo_root)

	build_dir = repo_root / "build"

	print(f"{Emoji.INFO} Building xfe_control_sim from main repo...")
	build_project(repo_root, build_dir, rebuild, verbose, build_shared_libs=False, build_executable=True)

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

def run_standalone_build(build_dir_name: str, test_type: str, rebuild: bool, verbose: bool = False) -> int:
	"""
    Default mode: Run as a standalone build script.
    
    This is what runs when the script is in a copied directory like sim_example.
    """
	script_dir = get_script_dir()
	source_dir = script_dir.parent

	repo_root = get_git_root()
	if repo_root is None:
		repo_root = source_dir
		print(f"{Colors.YELLOW}[INFO] Not in a git repository - using local directory structure{Colors.RESET}")

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

	build_project(source_dir, build_dir, rebuild, verbose, build_shared_libs, build_executable)

	run_clang_tidy_enabled = os.environ.get("RUN_CLANG_TIDY", "1")
	is_ci = is_ci_environment()

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
    Copy this script and related scripts from main repo to subdirectory.
    
    This ensures the subdirectory has the latest versions.
    """
	is_github_actions = os.environ.get("GITHUB_ACTIONS") == "true"
	is_ci = is_github_actions or os.environ.get("CI") is not None

	if is_ci:
		print(f"{Colors.YELLOW}Not syncing scripts since we are in CI{Colors.RESET}")
		return

	source_misc = repo_root / "misc"
	dest_misc = repo_root / subdir_name / "misc"

	if not dest_misc.exists():
		print(f"{Colors.YELLOW}[WARN] Destination misc directory not found: {dest_misc}{Colors.RESET}")
		return

	scripts_to_copy = ["launch_tests.py", "clang_format_all.py", "clang_tidy_all.py", "xflow_shared_functions.py"]

	print(f"{Emoji.INFO} Syncing scripts to {subdir_name}/misc/...")
	for script_name in scripts_to_copy:
		source_file = source_misc / script_name
		dest_file = dest_misc / script_name

		if source_file.exists():
			shutil.copy2(source_file, dest_file)
			print(f"   {Colors.GREEN}Copied {script_name}{Colors.RESET}")
		else:
			print(f"   {Colors.YELLOW}Skipped {script_name} (not found){Colors.RESET}")
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
	    description="Cross-platform build and test launcher",
	    epilog="""
Standalone build commands (default - run from any build directory):
  %(prog)s xfe_control_sim 1         Build and run xfe_control_sim (with rebuild)
  %(prog)s discon 0                  Build and run DISCON test (no rebuild)
  %(prog)s xfe_control_sim 0 -v      Build xfe_control_sim (no rebuild, verbose)

Main repo commands (run from xfe-control-sim/misc/):
  %(prog)s local_xfe_control_sim 1            Build and run in main repo (with rebuild)
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
		exit_code = run_main_repo_build(repo_root, rebuild, args.verbose)

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
		exit_code = run_standalone_build(args.build_dir, "xfe_control_sim", rebuild, args.verbose)

	elif args.command == "discon":
		exit_code = run_standalone_build(args.build_dir, "discon", rebuild, args.verbose)

	else:
		print(f"{Colors.RED}Unknown command: {args.command}{Colors.RESET}", file=sys.stderr)
		print(f"\n{Colors.YELLOW}Standalone commands: xfe_control_sim, discon{Colors.RESET}", file=sys.stderr)
		print(
		    f"{Colors.YELLOW}Main repo commands: local_xfe_control_sim, sim_example_copy_test, "
		    f"sim_example_copy_test_discon{Colors.RESET}",
		    file=sys.stderr)
		sys.exit(1)

	sys.exit(exit_code)

if __name__ == "__main__":
	main()
