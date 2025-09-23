# README

## Download not platform specific dependencies

- Download and install visual studio code
- In visual studio code download the following dependencies:
-- cpptools (c/++, c/c++ Extension Pack, C/C++ Themes)
-- cmake-tools
-- cmake-format
-- rainbow-csv

## Dependencies for Running the Program on Windows

- Download the installer from the latest release to install dependencies. 

## Installing Dependencies on unix systems, Macos and Linux

1. **Install via Homebrew** 

```bash
# 1. Install Homebrew (if not already installed)
# Open a terminal and run:

./misc/install_unix_dependencies.sh
```
```
1. **Install via linux**

Download the install shell script and run the shell script. 
```

## Running on VSCode

```text
Restart VSCode completely.
```

```text
Press Command + Shift + P, then:

CMake: Delete Cache and Reconfigure
CMake: Scan for Kits
CMake: Select a Kit:

on MACOS
# Choose the Homebrew LLVM Clang (not Apple Clang) 

On Windows choose the newly downloaded LLVM clang

To re-run Press Command + Shift + P, then: CMake: Delete Cache and Reconfigure

Build and run at the bottom left of VSCode. 

```text
---
