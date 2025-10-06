#!/usr/bin/env python3
"""
Cross-platform build script to create a standalone executable using PyInstaller.

This script automates the following steps:
1.  Detects the operating system (macOS, Linux, Windows).
2.  Verifies Python 3 is installed (and installs it via Homebrew on macOS if missing).
3.  Creates a Python virtual environment in './.venv' if it doesn't exist.
4.  Installs required build and runtime dependencies into the virtual environment.
5.  Checks for the main application script ('plot_viewer.py').
6.  Runs PyInstaller to build a single-file, windowed executable.
7.  Reports the path to the final executable and optionally launches it.
"""

import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

# --- Helper Classes & Functions (inspired by your shared utilities) ---

class Colors:
	"""ANSI color codes for terminal output."""
	RED = '\033[91m'
	GREEN = '\033[92m'
	YELLOW = '\033[93m'
	BLUE = '\033[94m'
	BOLD = '\033[1m'
	RESET = '\033[0m'

	@staticmethod
	def disable():
		"""Disable colors for non-TTY environments."""
		Colors.RED = Colors.GREEN = Colors.YELLOW = Colors.BLUE = ''
		Colors.BOLD = Colors.RESET = ''

class Emoji:
	"""Emoji characters for stylized output."""
	DETECT = '🔍'
	ARROW = '➡️ '
	APPLE = '🍎'
	PENGUIN = '🐧'
	WINDOWS = '🪟'
	CHECK = '✅'
	CROSS = '❌'
	BOX = '📦'
	GEAR = '⚙️'
	UP_ARROW = '⬆️'
	ROCKET = '🚀'

	@staticmethod
	def disable():
		"""Disable emojis for environments that don't support them well."""
		for attr in dir(Emoji):
			if not callable(getattr(Emoji, attr)) and not attr.startswith("__"):
				setattr(Emoji, attr, '')

def setup_display():
	"""Configure colors and emojis based on the environment."""
	if not sys.stdout.isatty() or (os.name == 'nt' and 'ANSICON' not in os.environ):
		Colors.disable()
	if os.name == 'nt':  # Emojis can be problematic on Windows terminals
		Emoji.disable()

def print_step(message):
	"""Prints a formatted step message."""
	print(f"\n{Colors.BOLD}{Colors.BLUE}--- {message} ---{Colors.RESET}")

def run_command(command: list[str], cwd: Path | None = None, check: bool = True):
	"""Runs a command, prints it, and handles errors."""
	print(f"{Colors.YELLOW}▶️  Running: {' '.join(command)}{Colors.RESET}")
	try:
		result = subprocess.run(
		    command, capture_output=True, text=True, encoding='utf-8', errors='replace', cwd=cwd, check=check)
		if result.stdout:
			print(result.stdout)
		if result.stderr:
			print(result.stderr, file=sys.stderr)
	except (subprocess.CalledProcessError, FileNotFoundError) as e:
		print(f"{Colors.RED}{Emoji.CROSS} Error running command: {e}{Colors.RESET}", file=sys.stderr)
		if isinstance(e, subprocess.CalledProcessError):
			print(f"{Colors.RED}{e.stderr}{Colors.RESET}", file=sys.stderr)
		sys.exit(1)

# --- Main Build Logic ---

def check_python_environment() -> str:
	"""
    Detects the OS and ensures a usable Python 3 interpreter is available.
    On macOS, it will attempt to install Python 3 via Homebrew if needed.
    Returns the recommended Python executable name ('python' or 'python3').
    """
	print_step(f"{Emoji.DETECT} Detecting Platform")
	system = platform.system()

	if system == "Darwin":
		print(f"{Emoji.APPLE} macOS detected.")
		if not shutil.which("brew"):
			print(
			    f"{Colors.RED}{Emoji.CROSS} Homebrew not found. Please install it from https://brew.sh/{Colors.RESET}")
			sys.exit(1)

		if not shutil.which("python3"):
			print_step(f"{Emoji.BOX} Installing Python 3 via Homebrew")
			run_command(["brew", "install", "python"])
		else:
			print(f"{Emoji.CHECK} Python 3 is already installed.")
		return "python3"

	elif system == "Linux":
		print(f"{Emoji.PENGUIN} Linux detected.")
		if not shutil.which("python3"):
			print(
			    f"{Colors.RED}{Emoji.CROSS} Python 3 is not installed. Please use your system package manager (e.g., apt, dnf, pacman).{Colors.RESET}"
			)
			sys.exit(1)
		return "python3"

	elif system == "Windows":
		print(f"{Emoji.WINDOWS} Windows detected.")
		# On Windows, 'python' is the standard command.
		if not shutil.which("python"):
			print(
			    f"{Colors.RED}{Emoji.CROSS} Python is not installed or not in PATH. Please install it from python.org or the Microsoft Store.{Colors.RESET}"
			)
			sys.exit(1)
		return "python"

	else:
		print(f"{Colors.RED}{Emoji.CROSS} Unsupported OS: {system}{Colors.RESET}")
		sys.exit(1)

def main():
	"""Main execution flow."""
	setup_display()

	# --- Configuration ---
	VENV_DIR = Path(".venv")
	MAIN_SCRIPT = "plot_viewer.py"
	DEPENDENCIES = ["pyinstaller", "pandas", "pyqtgraph", "PyQt5"]

	# 1. Check for a valid Python interpreter
	python_executable = check_python_environment()
	print(f"{Emoji.CHECK} Using host Python: {shutil.which(python_executable)}")

	# 2. Create or verify virtual environment
	print_step(f"{Emoji.GEAR} Setting up Virtual Environment")
	if not VENV_DIR.is_dir():
		print(f"Creating virtual environment in '{VENV_DIR}'...")
		run_command([python_executable, "-m", "venv", str(VENV_DIR)])
	else:
		print(f"{Emoji.CHECK} Virtual environment already exists at '{VENV_DIR}'.")

	# Determine paths for venv executables (cross-platform)
	if platform.system() == "Windows":
		venv_python = VENV_DIR / "Scripts" / "python.exe"
		venv_pip = VENV_DIR / "Scripts" / "pip.exe"
		venv_pyinstaller = VENV_DIR / "Scripts" / "pyinstaller.exe"
	else:
		venv_python = VENV_DIR / "bin" / "python"
		venv_pip = VENV_DIR / "bin" / "pip"
		venv_pyinstaller = VENV_DIR / "bin" / "pyinstaller"

	print(f"{Emoji.CHECK} Virtual environment activated (using direct paths).")

	# 3. Install dependencies
	print_step(f"{Emoji.UP_ARROW} Installing Dependencies")
	run_command([str(venv_pip), "install", "--upgrade", "pip"])
	print(f"Installing: {', '.join(DEPENDENCIES)}")
	run_command([str(venv_pip), "install", *DEPENDENCIES])
	print(f"{Emoji.CHECK} All dependencies installed successfully.")

	# 4. Verify main script exists
	print_step(f"{Emoji.DETECT} Verifying Main Script")
	if not Path(MAIN_SCRIPT).is_file():
		print(
		    f"{Colors.RED}{Emoji.CROSS} Main script '{MAIN_SCRIPT}' not found in the current directory.{Colors.RESET}")
		sys.exit(1)
	print(f"{Emoji.CHECK} Found main script: '{MAIN_SCRIPT}'")

	# 5. Run PyInstaller
	print_step(f"{Emoji.ROCKET} Building Standalone Executable")
	pyinstaller_args = [str(venv_pyinstaller), "--noconfirm", "--onefile", "--windowed", MAIN_SCRIPT]
	run_command(pyinstaller_args)

	# 6. Report and Launch
	executable_name = Path(MAIN_SCRIPT).stem
	if platform.system() == "Windows":
		executable_path = Path("dist") / f"{executable_name}.exe"
	else:
		# On macOS, PyInstaller creates an app bundle, but the executable is inside.
		# For simplicity, we point to the command-line starter in dist/.
		executable_path = Path("dist") / executable_name

	print_step(f"{Emoji.CHECK} Build Complete")
	if executable_path.exists():
		print(f"{Colors.GREEN}Standalone executable created at: {executable_path.resolve()}{Colors.RESET}")

		# Optionally, launch the new executable
		print_step(f"{Emoji.ROCKET} Launching Application")
		try:
			# Use Popen to launch the GUI application without blocking the script
			subprocess.Popen([str(executable_path.resolve())])
			print(f"{Emoji.CHECK} Application launched successfully.")
		except Exception as e:
			print(f"{Colors.RED}{Emoji.CROSS} Failed to launch the application: {e}{Colors.RESET}")
	else:
		print(
		    f"{Colors.RED}{Emoji.CROSS} Build failed. Executable not found at expected location: {executable_path}{Colors.RESET}"
		)
		sys.exit(1)

if __name__ == "__main__":
	main()
