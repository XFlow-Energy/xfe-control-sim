#!/bin/bash

set -e

echo "🔍 Detecting platform..."
OS="$(uname)"
echo "➡️ OS Detected: $OS"

# Choose Python binary name based on platform
PYTHON_BIN="python3"

if [[ "$OS" == "Darwin" ]]; then
	echo "🍎 macOS detected"
	if ! command -v brew &>/dev/null; then
		echo "❌ Homebrew not found. Please install Homebrew first: https://brew.sh/"
		exit 1
	fi
	if ! command -v python3 &>/dev/null; then
		echo "📦 Installing Python 3 via Homebrew..."
		brew install python
	else
		echo "✅ Python 3 is already installed via Homebrew"
	fi
	PYTHON_BIN="python3"

elif [[ "$OS" == "Linux" ]]; then
	echo "🐧 Linux detected"
	if ! command -v python3 &>/dev/null; then
		echo "❌ Python 3 is not installed. Please install it using your system package manager (e.g., apt, dnf, pacman)."
		exit 1
	fi
	PYTHON_BIN="python3"

else
	echo "❌ Unsupported OS: $OS"
	exit 1
fi

echo "✅ Using Python: $PYTHON_BIN"

# Virtual environment folder
VENV_DIR=".venv"

# Create virtual environment if it doesn't exist
if [[ ! -d "$VENV_DIR" ]]; then
	echo "⚙️ Creating virtual environment in $VENV_DIR"
	"$PYTHON_BIN" -m venv "$VENV_DIR"
else
	echo "✅ Virtual environment already exists"
fi

# Activate virtual environment
source "$VENV_DIR/bin/activate"
echo "✅ Virtual environment activated"

# Upgrade pip
echo "⬆️ Upgrading pip"
pip install --upgrade pip

# Install build dependencies (including PyInstaller and all runtime packages)
echo "🔍 Installing build dependencies (PyInstaller, pandas, pyqtgraph, PyQt5)..."
pip install pyinstaller pandas pyqtgraph PyQt5

# Verify main script exists
if [[ ! -f "plot_viewer.py" ]]; then
	echo "❌ plot_viewer.py not found in the current directory."
	deactivate
	exit 1
fi

# Run PyInstaller to produce one-file, windowed executable
echo "🚀 Building standalone executable with PyInstaller..."
pyinstaller --noconfirm --onefile --windowed plot_viewer.py

# The built executable will be in dist/
if [[ "$OS" == "Darwin" || "$OS" == "Linux" ]]; then
	EXECUTABLE="dist/plot_viewer"
else
	EXECUTABLE="dist/plot_viewer.exe"
fi

echo "✅ Standalone executable created at: $EXECUTABLE"

# Optionally, launch the new executable
echo "🚀 Launching the standalone executable..."
"$EXECUTABLE"

# Deactivate virtual environment and exit
deactivate