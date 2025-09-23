#!/usr/bin/env bash

# Detect OS
OS="$(uname -s)"

if [ "$OS" = "Darwin" ]; then
	# 1) Xcode CLT
	if ! xcode-select -p >/dev/null 2>&1; then
		echo "Installing Xcode Command-Line Tools…"
		xcode-select --install
		until xcode-select -p >/dev/null 2>&1; do sleep 5; done
	fi

	# 2) Homebrew
	if ! command -v brew >/dev/null 2>&1; then
		bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
		eval "$(/opt/homebrew/bin/brew shellenv)"
	fi

	# 3) GNU & project libs/tools
	if ! brew list git gsl cmake jansson libmodbus cppcheck include-what-you-use llvm z3 pcre tinyxml2 mpdecimal sqlite >/dev/null 2>&1; then
		brew update
		brew install git gsl cmake jansson libmodbus cppcheck include-what-you-use llvm z3 pcre tinyxml2 mpdecimal sqlite
	fi

	# Prepend Homebrew-LLVM so `clang`/`clang++` resolve to brew
	if [ -d /opt/homebrew/opt/llvm/bin ]; then
		export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
	fi

	# Ensure PATH entries exist in ~/.zshrc
	ZSHRC="$HOME/.zshrc"
	LLVM_LINE='export PATH="/opt/homebrew/opt/llvm/bin:${PATH}"'
	SQLITE_LINE='export PATH="/opt/homebrew/opt/sqlite/bin:${PATH}"'
	if [ ! -f "$ZSHRC" ]; then
		touch "$ZSHRC"
	fi
	if ! grep -Fq "$LLVM_LINE" "$ZSHRC"; then
		echo "$LLVM_LINE" >> "$ZSHRC"
	fi
	if ! grep -Fq "$SQLITE_LINE" "$ZSHRC"; then
		echo "$SQLITE_LINE" >> "$ZSHRC"
	fi

elif [ "$OS" = "Linux" ]; then
	# detect distro
	if [ -r /etc/os-release ]; then
		. /etc/os-release
		DISTRO="$ID"
	else
		DISTRO=""
	fi

	case "$DISTRO" in
		ubuntu|debian)
			echo "Detected Debian/Ubuntu – installing via apt"
			sudo apt-get update
			sudo apt-get install -y \
				build-essential \
				libgsl-dev \
				libjansson-dev \
				libmodbus-dev \
				cppcheck \
				iwyu \
				clang \
				llvm \
				z3 \
				sqlite3 \
				libsqlite3-dev
			;;
		fedora)
			echo "Detected Fedora – installing via dnf"
			sudo dnf install -y \
				gsl-devel \
				jansson-devel \
				libmodbus-devel \
				cppcheck \
				iwyu \
				clang \
				llvm \
				z3-devel \
				pcre-devel \
				sqlite-devel
			;;
		centos|rhel)
			echo "Detected CentOS/RHEL – installing via yum"
			sudo yum install -y epel-release
			sudo yum install -y \
				gsl-devel \
				jansson-devel \
				libmodbus-devel \
				cppcheck \
				iwyu \
				clang \
				llvm \
				z3-devel \
				pcre-devel \
				sqlite-devel
			;;
		arch)
			echo "Detected Arch – installing via pacman"
			sudo pacman -Sy --noconfirm \
				gsl \
				jansson \
				libmodbus \
				cppcheck \
				iwyu \
				llvm \
				z3 \
				pcre \
				sqlite
			;;
		opensus*|suse)
			echo "Detected openSUSE – installing via zypper"
			sudo zypper install -y \
				gsl-devel \
				jansson-devel \
				libmodbus-devel \
				cppcheck \
				iwyu \
				clang \
				z3-devel \
				pcre-devel \
				sqlite3-devel
			;;
		*)
			echo "Unsupported Linux distro: $DISTRO" >&2
			exit 1
			;;
	esac

else
	echo "Unsupported OS: $OS" >&2
	exit 1
fi