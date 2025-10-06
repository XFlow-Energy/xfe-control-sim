#!/usr/bin/env python3
"""
Cross-platform launcher for plot_viewer.py

Handles Python installation, virtual environment setup, dependency installation,
and launching the plot viewer application.
"""

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional, Tuple

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
	SEARCH = '🔍 '
	PACKAGE = '📦 '
	ROCKET = '🚀 '
	WRENCH = '⚙️ '
	ARROW = '➡️ '
	UPGRADE = '⬆️ '
	APPLE = '🍎 '
	PENGUIN = '🐧 '
	WINDOWS = '🪟 '

	@staticmethod
	def disable():
		"""Disable emojis for environments that don't support them."""
		Emoji.CHECK = '[OK]'
		Emoji.CROSS = '[FAIL]'
		Emoji.WARNING = '[WARN]'
		Emoji.INFO = '[INFO]'
		Emoji.SEARCH = '[SEARCH]'
		Emoji.PACKAGE = '[PKG]'
		Emoji.ROCKET = '[RUN]'
		Emoji.WRENCH = '[SETUP]'
		Emoji.ARROW = '-->'
		Emoji.UPGRADE = '[UP]'
		Emoji.APPLE = '[macOS]'
		Emoji.PENGUIN = '[Linux]'
		Emoji.WINDOWS = '[Win]'

def setup_display():
	"""Configure colors and emojis based on environment."""
	if not sys.stdout.isatty() or (os.name == 'nt' and not os.environ.get('ANSICON')):
		Colors.disable()

	# Disable emojis on Windows to avoid encoding issues
	if os.name == 'nt':
		Emoji.disable()

def get_python_info_windows() -> Tuple[Optional[str], Optional[str]]:
	"""
	Get Python executable and version on Windows.
	
	Returns:
		Tuple of (executable_name, version_string) or (None, None) if not found.
	"""
	# Try 'py' launcher first
	for exe, args in [("py", ["-3", "--version"]), ("python", ["--version"])]:
		if shutil.which(exe):
			try:
				result = subprocess.run(
				    [exe] + args, capture_output=True, text=True, encoding='utf-8', errors='replace', check=False)

				# Combine stdout and stderr since --version may output to either
				output = (result.stdout + result.stderr).strip()

				if output.startswith("Python") and result.returncode == 0:
					return (exe, output)
			except Exception:
				continue

	return (None, None)

def install_python_windows(install_dir: Path) -> bool:
	"""
	Download and install Python on Windows.
	
	Args:
		install_dir: Directory to install Python to.
		
	Returns:
		True if successful, False otherwise.
	"""
	installer_url = "https://www.python.org/ftp/python/3.11.4/python-3.11.4-amd64.exe"
	installer_path = Path.cwd() / "python-installer.exe"

	print(f"{Emoji.PACKAGE} Downloading Python installer...")
	try:
		subprocess.run(
		    ["powershell", "-Command", f"Invoke-WebRequest -Uri '{installer_url}' -OutFile '{installer_path}'"],
		    check=True)
	except subprocess.CalledProcessError:
		print(f"{Colors.RED}Failed to download Python installer{Colors.RESET}", file=sys.stderr)
		return False

	print(f"{Emoji.WRENCH} Installing Python silently (may require admin privileges)...")
	try:
		subprocess.run([str(installer_path), "/quiet", "InstallAllUsers=1", "PrependPath=1"], check=True)
	except subprocess.CalledProcessError:
		print(f"{Colors.RED}Python installation failed{Colors.RESET}", file=sys.stderr)
		return False
	finally:
		if installer_path.exists():
			installer_path.unlink()

	print(f"{Emoji.INFO} Refreshing PATH...")
	# Refresh environment PATH
	if os.name == 'nt':
		import winreg
		with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
		                    r'SYSTEM\CurrentControlSet\Control\Session Manager\Environment') as key:
			path = winreg.QueryValueEx(key, 'Path')[0]
			os.environ['PATH'] = path

	return True

def check_python() -> Tuple[str, str]:
	"""
	Check for Python installation and return executable and version.
	
	Returns:
		Tuple of (python_executable, version_string).
		
	Raises:
		SystemExit if Python cannot be found or installed.
	"""
	system = platform.system()

	if system == "Windows":
		python_exe, python_ver = get_python_info_windows()

		if not python_exe:
			print(f"{Emoji.WARNING} Python not found on Windows.")
			response = input("Would you like to automatically install Python 3.11.4? (y/n): ")

			if response.lower() == 'y':
				install_dir = Path("C:/Python311")
				if install_python_windows(install_dir):
					# Re-check after installation
					python_exe, python_ver = get_python_info_windows()
					if not python_exe:
						print(
						    f"{Colors.RED}Python was installed but still not found. "
						    f"Please restart your terminal.{Colors.RESET}",
						    file=sys.stderr)
						sys.exit(1)
				else:
					sys.exit(1)
			else:
				print(f"{Colors.RED}Python is required. Please install Python 3 and try again.{Colors.RESET}")
				sys.exit(1)

		return (python_exe, python_ver)

	elif system == "Darwin":
		print(f"{Emoji.APPLE} macOS detected")

		if not shutil.which("python3"):
			if not shutil.which("brew"):
				print(
				    f"{Colors.RED}Homebrew not found. Please install Homebrew first: "
				    f"https://brew.sh/{Colors.RESET}",
				    file=sys.stderr)
				sys.exit(1)

			print(f"{Emoji.PACKAGE} Installing Python 3 via Homebrew...")
			subprocess.run(["brew", "install", "python"], check=True)
		else:
			print(f"{Emoji.CHECK} Python 3 is already installed")

		result = subprocess.run(
		    ["python3", "--version"], capture_output=True, text=True, encoding='utf-8', errors='replace', check=True)
		return ("python3", result.stdout.strip())

	elif system == "Linux":
		print(f"{Emoji.PENGUIN} Linux detected")

		if not shutil.which("python3"):
			print(
			    f"{Colors.RED}Python 3 is not installed. Please install it using your "
			    f"system package manager (e.g., apt, dnf, pacman).{Colors.RESET}",
			    file=sys.stderr)
			sys.exit(1)

		result = subprocess.run(
		    ["python3", "--version"], capture_output=True, text=True, encoding='utf-8', errors='replace', check=True)
		return ("python3", result.stdout.strip())

	else:
		print(f"{Colors.RED}Unsupported OS: {system}{Colors.RESET}", file=sys.stderr)
		sys.exit(1)

def create_virtualenv(python_exe: str, venv_dir: Path) -> Path:
	"""
	Create a virtual environment if it doesn't exist.
	
	Args:
		python_exe: Python executable name or path.
		venv_dir: Directory for the virtual environment.
		
	Returns:
		Path to the activation script.
	"""
	if not venv_dir.exists():
		print(f"{Emoji.WRENCH} Creating virtual environment in {venv_dir}")

		if python_exe == "py":
			subprocess.run([python_exe, "-3", "-m", "venv", str(venv_dir)], check=True)
		else:
			subprocess.run([python_exe, "-m", "venv", str(venv_dir)], check=True)
	else:
		print(f"{Emoji.CHECK} Virtual environment already exists")

	# Return activation script path
	if platform.system() == "Windows":
		activate_script = venv_dir / "Scripts" / "Activate.ps1"
		if not activate_script.exists():
			print(
			    f"{Colors.RED}Failed to create virtual environment. "
			    f"Cannot find Activate.ps1.{Colors.RESET}",
			    file=sys.stderr)
			sys.exit(1)
		return activate_script
	else:
		return venv_dir / "bin" / "activate"

def get_venv_python(venv_dir: Path) -> Path:
	"""Get the path to the Python executable inside the virtual environment."""
	if platform.system() == "Windows":
		return venv_dir / "Scripts" / "python.exe"
	else:
		return venv_dir / "bin" / "python"

def install_packages(venv_python: Path, packages: list, check_installed: bool = True):
	"""
	Install required Python packages in the virtual environment.
	
	Args:
		venv_python: Path to the venv Python executable.
		packages: List of package names to install.
		check_installed: If True, check if package is already installed before installing.
	"""
	print(f"{Emoji.UPGRADE} Upgrading pip...")
	subprocess.run([str(venv_python), "-m", "pip", "install", "--upgrade", "pip"], check=True)

	if check_installed:
		print(f"{Emoji.SEARCH} Checking and installing required packages...")
		for pkg in packages:
			print(f"Checking {pkg}...")

			# Try to import the package
			result = subprocess.run([str(venv_python), "-c", f"import {pkg}"], capture_output=True, check=False)

			if result.returncode == 0:
				print(f" {Emoji.CHECK} {pkg} already installed.")
			else:
				print(f" {Emoji.PACKAGE} Installing {pkg}...")
				subprocess.run([str(venv_python), "-m", "pip", "install", pkg], check=True)
	else:
		print(f"{Emoji.PACKAGE} Installing packages: {', '.join(packages)}...")
		subprocess.run([str(venv_python), "-m", "pip", "install"] + packages, check=True)

def build_executable(venv_python: Path, script_path: Path) -> Path:
	"""
	Build a standalone executable using PyInstaller.
	
	Args:
		venv_python: Path to the venv Python executable.
		script_path: Path to the Python script to package.
		
	Returns:
		Path to the created executable.
	"""
	print(f"{Emoji.ROCKET} Building standalone executable with PyInstaller...")

	subprocess.run(
	    [str(venv_python), "-m", "PyInstaller", "--noconfirm", "--onefile", "--windowed",
	     str(script_path)], check=True)

	# Determine executable path
	if platform.system() == "Windows":
		executable = Path("dist") / f"{script_path.stem}.exe"
	else:
		executable = Path("dist") / script_path.stem

	if not executable.exists():
		print(f"{Colors.RED}Executable not found at {executable}{Colors.RESET}", file=sys.stderr)
		sys.exit(1)

	print(f"{Emoji.CHECK} Standalone executable created at: {executable}")
	return executable

def launch_script(venv_python: Path, script_path: Path):
	"""
	Launch the Python script directly using the virtual environment Python.
	
	Args:
		venv_python: Path to the venv Python executable.
		script_path: Path to the Python script to run.
	"""
	print(f"{Emoji.ROCKET} Launching {script_path}...")
	subprocess.run([str(venv_python), str(script_path)], check=True)

def main():
	"""Main entry point."""
	setup_display()

	parser = argparse.ArgumentParser(
	    description="Cross-platform launcher for plot_viewer.py",
	    formatter_class=argparse.RawDescriptionHelpFormatter,
	    epilog="""
Examples:
  %(prog)s                    # Run plot_viewer.py directly
  %(prog)s --build            # Build standalone executable and run it
  %(prog)s --venv-dir .venv   # Use custom venv directory
		""")

	parser.add_argument(
	    "--build", action="store_true", help="Build standalone executable with PyInstaller before running")
	parser.add_argument(
	    "--venv-dir",
	    type=Path,
	    default=None,
	    help="Virtual environment directory (default: .venv on Unix, C:\\.venv on Windows)")
	parser.add_argument(
	    "--script",
	    type=Path,
	    default=Path("plot_viewer.py"),
	    help="Path to the plot viewer script (default: plot_viewer.py)")
	parser.add_argument(
	    "--skip-install-check", action="store_true", help="Skip checking if packages are already installed")

	args = parser.parse_args()

	# Detect platform
	system = platform.system()
	print(f"{Emoji.SEARCH} Detecting platform...")
	print(f"{Emoji.ARROW} OS Detected: {system}")

	# Check for Python
	python_exe, python_ver = check_python()
	print(f"{Emoji.CHECK} Using Python: {python_exe} ({python_ver})")

	# Determine venv directory
	if args.venv_dir:
		venv_dir = args.venv_dir
	else:
		if system == "Windows":
			venv_dir = Path("C:/.venv")
		else:
			venv_dir = Path(".venv")

	# Create virtual environment
	create_virtualenv(python_exe, venv_dir)
	venv_python = get_venv_python(venv_dir)

	print(f"{Emoji.CHECK} Virtual environment activated")

	# Determine required packages
	required_packages = ["pandas", "pyqtgraph", "PyQt5"]
	if args.build:
		required_packages.append("pyinstaller")

	# Install packages
	install_packages(venv_python, required_packages, check_installed=not args.skip_install_check)

	# Verify script exists
	if not args.script.exists():
		print(f"{Colors.RED}{args.script} not found in the current directory.{Colors.RESET}", file=sys.stderr)
		sys.exit(1)

	# Build or run
	if args.build:
		executable = build_executable(venv_python, args.script)
		print(f"{Emoji.ROCKET} Launching the standalone executable...")
		subprocess.run([str(executable)], check=True)
	else:
		launch_script(venv_python, args.script)

	print(f"{Emoji.CHECK} Complete")

if __name__ == "__main__":
	main()
