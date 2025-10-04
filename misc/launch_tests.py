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

def get_script_dir():
	"""Get the directory containing this script."""
	return Path(__file__).parent.resolve()

def get_git_root():
	"""Get the git repository root directory. Returns None if not in a git repo."""
	try:
		result = subprocess.run(["git", "rev-parse", "--show-toplevel"], capture_output=True, text=True, check=True)
		return Path(result.stdout.strip())
	except (subprocess.CalledProcessError, FileNotFoundError):
		return None

def get_nproc():
	"""Get number of processor cores."""
	system = platform.system()
	if system == "Darwin":
		result = subprocess.run(["sysctl", "-n", "hw.ncpu"], capture_output=True, text=True, check=True)
		return int(result.stdout.strip())
	elif system == "Windows":
		return os.cpu_count() or 1
	else:  # Linux
		return os.cpu_count() or 1

def check_command_exists(cmd):
	"""Check if a command exists in PATH."""
	return shutil.which(cmd) is not None

def run_yapf(repo_root):
	"""Run yapf to format all Python files in the repository."""

	# Determine if we're in CI
	is_github_actions = os.environ.get("GITHUB_ACTIONS") == "true"
	is_ci = is_github_actions or os.environ.get("CI") is not None

	if is_ci:
		print("Not running since we are in CI")
		return

	if not check_command_exists("yapf"):
		print("[WARN] yapf not found in PATH; skipping Python formatting.")
		return

	print("-> Running yapf to format Python files...")
	result = subprocess.run(["yapf", "-i", "-r", "."], cwd=repo_root, capture_output=True, text=True)

	if result.returncode != 0:
		print(f"[WARN] yapf returned non-zero exit code: {result.returncode}")
		if result.stderr:
			print(result.stderr)
	else:
		print("yapf completed successfully")
	print()

def run_clang_format(repo_root):
	"""Run clang-format on all source files."""
	clang_format_script = repo_root / "misc" / "clang_format_all.py"

	if not clang_format_script.exists():
		print(f"[WARN] clang-format script not found at {clang_format_script}; skipping.")
		return

	if not check_command_exists("clang-format"):
		print("[WARN] clang-format not found in PATH; skipping clang-format step.")
		return

	print("-> Running clang-format on all source files...")
	result = subprocess.run([sys.executable, str(clang_format_script)], cwd=repo_root)

	if result.returncode != 0:
		print(f"[WARN] clang-format returned non-zero exit code: {result.returncode}")
	else:
		print("clang-format completed successfully")
	print()

def build_project(source_dir, build_dir, rebuild, verbose=False, build_shared_libs=False, build_executable=True):
	"""Build the project using CMake."""

	# Determine if we're in CI
	is_github_actions = os.environ.get("GITHUB_ACTIONS") == "true"
	is_ci = is_github_actions or os.environ.get("CI") is not None

	if rebuild:
		if build_dir.exists():
			print(f"-> Removing {build_dir}")
			shutil.rmtree(build_dir)

		# Remove cppcheck cache
		cache_dir = Path.home() / ".cache" / "cppcheck"
		if cache_dir.exists():
			shutil.rmtree(cache_dir)

		build_dir.mkdir(parents=True, exist_ok=True)

		# Determine number of processors
		nproc = get_nproc()

		# Detect build system
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

		# Determine OS
		if is_github_actions:
			os_name = os.environ.get("RUNNER_OS", platform.system())
		else:
			os_name = platform.system()

		# Select compilers
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
				print(f"Unsupported OS: {os_name}", file=sys.stderr)
				sys.exit(1)
		else:
			# Local build
			if platform.system() == "Darwin":
				cc = "/opt/homebrew/opt/llvm/bin/clang"
				cxx = "/opt/homebrew/opt/llvm/bin/clang++"
			else:
				cc = "clang"
				cxx = "clang++"

		# CMake verbose flag
		cmake_verbose = "ON" if verbose else "OFF"

		# CI-only prefix path for Windows dependencies
		cmake_prefix_path = ""
		if is_github_actions and os_name == "Windows":
			cmake_prefix_path = "C:/deps/gsl-install;C:/deps/jansson-install;C:/deps/libmodbus"

		# Build type
		build_type = "Release" if is_ci else "Debug"
		if not build_executable:  # For DISCON test, use Release
			build_type = "Release"
		else:
			build_type = "Debug"  # Override for main executable

		# CMake configuration
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

		print(f"-> Configuring with CMake...")
		subprocess.run(cmake_cmd, check=True, cwd=source_dir)

		print(f"-> Building with {nproc} processors...")
		subprocess.run(build_cmd, check=True, cwd=build_dir)

def run_clang_tidy(repo_root, source_dir, build_dir):
	"""Run clang-tidy on the project."""
	is_ci = os.environ.get("GITHUB_ACTIONS") == "true" or os.environ.get("CI") is not None

	clang_tidy_script = repo_root / "misc" / "clang_tidy_all.py"
	if not clang_tidy_script.exists():
		print(f"[WARN] clang-tidy script not found at {clang_tidy_script}; skipping.")
		return

	clang_tidy_log = build_dir / "clang-tidy.log"
	clang_tidy_file = os.environ.get("CLANG_TIDY_FILE", str(repo_root / ".clang-tidy"))

	if clang_tidy_log.exists():
		clang_tidy_log.unlink()

	print(f"[INFO] Running clang-tidy… logging to {clang_tidy_log}")
	print(f"[INFO] Using config file: {clang_tidy_file}")

	# Set environment variables for the clang_tidy script
	env = os.environ.copy()
	env["PROJECT_ROOT"] = str(source_dir)
	env["BUILD_DIR"] = str(build_dir)

	mode = os.environ.get("RUN_CLANG_TIDY_MODE", "c")

	cmd = [sys.executable, str(clang_tidy_script), mode, clang_tidy_file]
	if is_ci:
		cmd.extend(["--extraargs", "-warnings-as-errors='*'"])

	with open(clang_tidy_log, "w", encoding='utf-8') as log_file:
		result = subprocess.run(
		    cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env, encoding='utf-8')
		output = result.stdout
		print(output, end="")
		log_file.write(output)

		if result.returncode != 0:
			print(f"[WARN] clang-tidy returned non-zero; see {clang_tidy_log}")

def run_binary(source_dir, build_dir, binary_name):
	"""Run the compiled binary and return exit code."""
	# Ensure log directory exists
	log_dir = source_dir / "log" / "log_data"
	log_dir.mkdir(parents=True, exist_ok=True)

	# Find binary
	bin_dir = build_dir / "executables-out"
	if not bin_dir.exists():
		print(f"Binary directory not found: {bin_dir}", file=sys.stderr)
		return 1

	bin_path = bin_dir / binary_name
	if not bin_path.exists():
		bin_path = bin_dir / f"{binary_name}.exe"
		if not bin_path.exists():
			print(f"Binary not found: {binary_name}", file=sys.stderr)
			return 1

	print(f"-> Running {bin_path}...")
	result = subprocess.run(
	    [str(bin_path)], cwd=bin_dir, capture_output=True, text=True, encoding='utf-8', errors='replace')

	print(result.stdout)
	if result.stderr:
		print(result.stderr, file=sys.stderr)
	print()

	return result.returncode

def validate_log_file(log_file, test_type):
	"""Validate the log file contents."""
	if not log_file.exists():
		print(f"Log file not found: {log_file}", file=sys.stderr)
		return False

	print(f"-> Validating log file: {log_file}")

	with open(log_file, "r", encoding='utf-8', errors='replace') as f:
		content = f.read()
		lines = [line for line in content.split("\n") if line.strip()]

	log_ok = True

	if test_type == "discon":
		if "discon init complete!" not in content:
			print("Missing 'discon init complete!' line.", file=sys.stderr)
			log_ok = False
	else:  # xfe_control_sim
		if "Program Duration:" not in content:
			print("Missing 'Program Duration:' line.", file=sys.stderr)
			log_ok = False

		if "write Duration:" not in content:
			print("Missing 'write Duration:' line.", file=sys.stderr)
			log_ok = False

		if lines:
			last_line = lines[-1]
			if "Closing Program" not in last_line:
				print("Last non-empty line is not 'Closing Program'.", file=sys.stderr)
				print(f"   Last line was: {last_line}", file=sys.stderr)
				log_ok = False

	if "ERROR" in content:
		print("Found error lines in log:", file=sys.stderr)
		for line in content.split("\n"):
			if "ERROR" in line:
				print(line, file=sys.stderr)
		log_ok = False

	return log_ok

def sync_scripts_to_subdir(repo_root, subdir_name):
	"""
    Copy this script and related scripts from main repo to subdirectory.
    This ensures the subdirectory has the latest versions.
    """

	# Determine if we're in CI
	is_github_actions = os.environ.get("GITHUB_ACTIONS") == "true"
	is_ci = is_github_actions or os.environ.get("CI") is not None

	if is_ci:
		print("Not syncing scripts since we are in CI")
		return

	source_misc = repo_root / "misc"
	dest_misc = repo_root / subdir_name / "misc"

	if not dest_misc.exists():
		print(f"[WARN] Destination misc directory not found: {dest_misc}")
		return

	scripts_to_copy = ["launch_tests.py", "clang_format_all.py", "clang_tidy_all.py"]

	print(f"-> Syncing scripts to {subdir_name}/misc/...")
	for script_name in scripts_to_copy:
		source_file = source_misc / script_name
		dest_file = dest_misc / script_name

		if source_file.exists():
			shutil.copy2(source_file, dest_file)
			print(f"Copied {script_name}")
		else:
			print(f"Skipped {script_name} (not found)")
	print()

def run_standalone_build(build_dir_name, test_type, rebuild, verbose=False):
	"""
    Default mode: Run as a standalone build script.
    This is what runs when the script is in a copied directory like sim_example.
    """
	script_dir = get_script_dir()
	source_dir = script_dir.parent  # Go up from misc/ to project root

	# Try to find the main repo (if we're in a subdirectory of it)
	repo_root = get_git_root()
	if repo_root is None:
		# Not in a git repo - use source_dir as repo_root for paths
		repo_root = source_dir
		print("[INFO] Not in a git repository - using local directory structure")

	# Run clang-format if we can find it
	run_clang_format(repo_root)

	build_dir = source_dir / build_dir_name

	if test_type == "discon":
		print("-> Building DISCON interface test...")
		binary_name = "qblade_interface_test"
		build_shared_libs = True
		build_executable = False
	else:  # xfe_control_sim
		print("-> Building xfe_control_sim...")
		binary_name = "xfe_control_sim"
		build_shared_libs = False
		build_executable = True

	build_project(source_dir, build_dir, rebuild, verbose, build_shared_libs, build_executable)

	# Run clang-tidy if enabled
	run_clang_tidy_enabled = os.environ.get("RUN_CLANG_TIDY", "1")
	is_ci = os.environ.get("GITHUB_ACTIONS") == "true" or os.environ.get("CI") is not None

	if is_ci or run_clang_tidy_enabled == "1":
		run_clang_tidy(repo_root, source_dir, build_dir)
	else:
		print("[INFO] RUN_CLANG_TIDY not set; skipping clang-tidy step.")

	print()

	# Run binary
	exit_code = run_binary(source_dir, build_dir, binary_name)

	if test_type == "discon":
		if exit_code != 0:
			print(f"qblade interface test failed (exit {exit_code})", file=sys.stderr)
		else:
			print("qblade interface test passed!")

	# Print log file
	log_file = source_dir / "log" / "log_data" / "xfe-control-sim-simulation-output.log"
	if log_file.exists():
		print("-> Contents of simulation log:")
		with open(log_file, "r", encoding='utf-8', errors='replace') as f:
			print(f.read())
		print()
	else:
		print(f"Log file not found: {log_file}")

	# Validate log for xfe_control_sim (DISCON test doesn't validate the same way)
	if test_type == "xfe_control_sim":
		log_ok = validate_log_file(log_file, "xfe_control_sim")
		if log_ok:
			print("Log validation passed")
		if not log_ok and exit_code == 0:
			exit_code = 1

	return exit_code

def run_main_repo_build(repo_root, rebuild, verbose=False):
	"""
    Run build from the main xfe-control-sim repo (local_xfe_control_sim command).
    """
	# First, run yapf on Python files
	run_yapf(repo_root)

	# Then sync scripts to sim_example (or any subdirs that need them)
	sync_scripts_to_subdir(repo_root, "sim_example")

	run_clang_format(repo_root)

	build_dir = repo_root / "build"

	print("-> Building xfe_control_sim from main repo...")
	build_project(repo_root, build_dir, rebuild, verbose, build_shared_libs=False, build_executable=True)

	# Run clang-tidy if enabled
	run_clang_tidy_enabled = os.environ.get("RUN_CLANG_TIDY", "1")
	is_ci = os.environ.get("GITHUB_ACTIONS") == "true" or os.environ.get("CI") is not None

	if is_ci or run_clang_tidy_enabled == "1":
		run_clang_tidy(repo_root, repo_root, build_dir)
	else:
		print("[INFO] RUN_CLANG_TIDY not set; skipping clang-tidy step.")

	print()

	# Run binary
	exit_code = run_binary(repo_root, build_dir, "xfe_control_sim")

	# Print log file
	log_file = repo_root / "log" / "log_data" / "xfe-control-sim-simulation-output.log"
	if log_file.exists():
		print("-> Contents of simulation log:")
		with open(log_file, "r", encoding='utf-8', errors='replace') as f:
			print(f.read())
		print()
	else:
		print(f"Log file not found: {log_file}")

	# Validate log
	log_ok = validate_log_file(log_file, "xfe_control_sim")
	if log_ok:
		print("Log validation passed")
	if not log_ok and exit_code == 0:
		exit_code = 1

	return exit_code

def run_copy_test(repo_root, subdir_name, test_type, rebuild):
	"""
    Run test by copying a subdirectory to a temp location and running there.
    Used for sim_example_copy_test and sim_example_copy_test_discon.
    """
	# First, run yapf on Python files
	run_yapf(repo_root)

	# Then sync scripts to the subdirectory before copying
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

	print(f"-> Testing {test_name} in temporary dir: {tmp_root}")

	# Recreate or skip
	if rebuild:
		if tmp_root.exists():
			shutil.rmtree(tmp_root)

	if not tmp_root.exists():
		print(f"-> Copying {subdir_name} to {tmp_root}")
		shutil.copytree(source_subdir, tmp_root)

	# Build + run via this same script in the copied location
	print(f"-> Building + running via {tmp_root}/misc/launch_tests.py")

	launcher_path = tmp_root / "misc" / "launch_tests.py"
	if not launcher_path.exists():
		print(f"Launcher script not found: {launcher_path}", file=sys.stderr)
		print(f"   Make sure launch_tests.py is copied to {subdir_name}/misc/", file=sys.stderr)
		return 1

	result = subprocess.run(
	    [sys.executable, str(launcher_path), command_arg, "1" if rebuild else "0"], cwd=tmp_root / "misc")
	launch_exit = result.returncode

	# Outcome
	if launch_exit != 0:
		print(f"{test_name} test run failed (exit {launch_exit})", file=sys.stderr)
	else:
		print(f"{test_name} test run succeeded")

	# Validate log file
	log_file = tmp_root / "log" / "log_data" / "xfe-control-sim-simulation-output.log"
	log_ok = validate_log_file(log_file, test_type)

	os.chdir(repo_root)

	if log_ok:
		print(f"Log validation passed. Cleaning up temp folder: {tmp_root}")
		shutil.rmtree(tmp_root)
		return launch_exit
	else:
		print(f" Log validation failed. Preserving temp folder for inspection: {tmp_root}", file=sys.stderr)
		print(f"   You can inspect the log with: less '{log_file}'", file=sys.stderr)
		return launch_exit if launch_exit != 0 else 1

def main():
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

	# If no command provided, show help
	if args.command is None:
		parser.print_help()
		sys.exit(0)

	rebuild = args.rebuild == "1"

	# Main repo commands (special behavior)
	if args.command == "local_xfe_control_sim":
		repo_root = get_git_root()
		if repo_root is None:
			print("Error: local_xfe_control_sim requires being in a git repository", file=sys.stderr)
			sys.exit(1)
		exit_code = run_main_repo_build(repo_root, rebuild, args.verbose)

	elif args.command == "sim_example_copy_test":
		repo_root = get_git_root()
		if repo_root is None:
			print("Error: sim_example_copy_test requires being in a git repository", file=sys.stderr)
			sys.exit(1)
		exit_code = run_copy_test(repo_root, args.subdir, "xfe_control_sim", rebuild)

	elif args.command == "sim_example_copy_test_discon":
		repo_root = get_git_root()
		if repo_root is None:
			print("Error: sim_example_copy_test_discon requires being in a git repository", file=sys.stderr)
			sys.exit(1)
		exit_code = run_copy_test(repo_root, args.subdir, "discon", rebuild)

	# Standalone build commands (default behavior)
	elif args.command == "xfe_control_sim":
		exit_code = run_standalone_build(args.build_dir, "xfe_control_sim", rebuild, args.verbose)

	elif args.command == "discon":
		exit_code = run_standalone_build(args.build_dir, "discon", rebuild, args.verbose)

	else:
		print(f"Unknown command: {args.command}", file=sys.stderr)
		print("\nStandalone commands: xfe_control_sim, discon", file=sys.stderr)
		print(
		    "Main repo commands: local_xfe_control_sim, sim_example_copy_test, sim_example_copy_test_discon",
		    file=sys.stderr)
		sys.exit(1)

	sys.exit(exit_code)

if __name__ == "__main__":
	main()
