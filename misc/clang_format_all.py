"""
Cross-platform clang-format runner.
Supports: format (default), dry-run, and pre-commit modes.
"""

import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List, Optional, Set, Tuple

from xflow_shared_functions import Colors, Emoji, setup_display, find_file

# Initialize display settings
setup_display()

def check_needs_formatting(clang_format_cmd: str,
                           files: List[str],
                           style_arg: str,
                           verbose: bool = False) -> Tuple[bool, List[str]]:
	"""
    Check if any files need formatting by comparing output with original.
    
    Returns:
        Tuple of (needs_formatting: bool, files_needing_format: list)
    """
	files_needing_format = []

	for file_path in files:
		if verbose:
			print(f"Checking {file_path}...", end=' ')

		try:
			with open(file_path, 'r', encoding='utf-8') as f:
				original_content = f.read()
		except (IOError, UnicodeDecodeError) as e:
			print(f"{Colors.YELLOW}Warning: Could not read {file_path}: {e}{Colors.RESET}", file=sys.stderr)
			if verbose:
				print(f"{Colors.YELLOW}SKIP{Colors.RESET}")
			continue

		result = subprocess.run([clang_format_cmd, style_arg, file_path], capture_output=True, text=True)

		if result.returncode != 0:
			print(f"{Colors.YELLOW}Warning: clang-format failed on {file_path}{Colors.RESET}", file=sys.stderr)
			if result.stderr:
				print(result.stderr, file=sys.stderr)
			if verbose:
				print(f"{Colors.YELLOW}ERROR{Colors.RESET}")
			continue

		formatted_content = result.stdout

		if original_content != formatted_content:
			files_needing_format.append(file_path)
			if verbose:
				print(f"{Colors.RED}NEEDS FORMAT{Colors.RESET}")
		elif verbose:
			print(f"{Colors.GREEN}OK{Colors.RESET}")

	return len(files_needing_format) > 0, files_needing_format

def find_clang_format() -> Optional[str]:
	"""Find clang-format executable."""
	if 'CLANG_FORMAT' in os.environ:
		cmd = os.environ['CLANG_FORMAT']
		if shutil.which(cmd):
			return cmd
		print(
		    f"{Colors.YELLOW}Warning: CLANG_FORMAT env var set to '{cmd}' "
		    f"but not found{Colors.RESET}",
		    file=sys.stderr)

	candidates = ['clang-format', 'clang-format-18', 'clang-format-17', 'clang-format-16', 'clang-format-15']

	for cmd in candidates:
		if shutil.which(cmd):
			return cmd

	return None

def format_files(
        clang_format_cmd: str, files: List[str], style_arg: str, in_place: bool = True, verbose: bool = False) -> bool:
	"""Format the specified files."""
	args = [clang_format_cmd]
	if in_place:
		args.append('-i')
	args.append(style_arg)

	for file_path in files:
		if verbose:
			print(f"Formatting {file_path}...")

		result = subprocess.run([*args, file_path], capture_output=True, text=True)

		if result.returncode != 0:
			print(f"{Colors.RED}Error formatting {file_path}:{Colors.RESET}", file=sys.stderr)
			print(result.stderr, file=sys.stderr)
			return False

	return True

def get_source_files(root_dir: str,
                     extensions: Optional[Set[str]] = None,
                     exclude_dirs: Optional[Set[str]] = None) -> List[str]:
	"""Find all C/C++ source and header files."""
	if extensions is None:
		extensions = {'.c', '.h', '.cpp', '.hpp', '.cc', '.cxx', '.hxx', '.inl'}

	if exclude_dirs is None:
		exclude_dirs = {
		    'build', '.git', 'cmake-build-debug', 'cmake-build-release', 'third_party', 'external', 'vendor', '.venv',
		    'venv'
		}

	if 'CLANG_FORMAT_EXCLUDE' in os.environ:
		exclude_dirs.update(os.environ['CLANG_FORMAT_EXCLUDE'].split(','))

	files = []
	root_path = Path(root_dir)

	for path in root_path.rglob('*'):
		if path.is_file() and path.suffix in extensions:
			if not any(excluded in path.parts for excluded in exclude_dirs):
				files.append(str(path))

	return sorted(files)

def main():
	"""Main entry point."""
	mode = 'format'
	verbose = False
	args = sys.argv[1:]

	if '-v' in args or '--verbose' in args:
		verbose = True
		args = [a for a in args if a not in ['-v', '--verbose']]

	if '-h' in args or '--help' in args:
		print(
		    f"""
{Colors.BOLD}Usage:{Colors.RESET} {sys.argv[0]} [mode] [options]

{Colors.BOLD}Modes:{Colors.RESET}
  format      Format all files in-place (default)
  dry-run     Check if files need formatting (exit 1 if they do)
  pre-commit  Format files and abort commit to review changes

{Colors.BOLD}Options:{Colors.RESET}
  -v, --verbose  Show detailed progress
  -h, --help     Show this help message

{Colors.BOLD}Environment Variables:{Colors.RESET}
  CLANG_FORMAT              Path to clang-format executable
  CLANG_FORMAT_STYLE        Path to .clang-format file or style name
  CLANG_FORMAT_EXCLUDE      Comma-separated list of directories to exclude

{Colors.BOLD}Examples:{Colors.RESET}
  {sys.argv[0]} format
  {sys.argv[0]} dry-run
  CLANG_FORMAT_STYLE=Google {sys.argv[0]} format
        """)
		sys.exit(0)

	if len(args) > 0:
		if args[0] in ['dry-run', 'pre-commit', 'format']:
			mode = args[0]
		else:
			print(f"{Colors.RED}Unknown mode: {args[0]}{Colors.RESET}", file=sys.stderr)
			print(f"Run '{sys.argv[0]} --help' for more information", file=sys.stderr)
			sys.exit(1)

	clang_format_cmd = find_clang_format()
	if not clang_format_cmd:
		print(f"{Colors.RED}Error: clang-format not found in PATH{Colors.RESET}", file=sys.stderr)
		print("Install clang-format or set CLANG_FORMAT environment variable", file=sys.stderr)
		sys.exit(1)

	if verbose:
		print(f"Using clang-format: {clang_format_cmd}")

	try:
		result = subprocess.run(['git', 'rev-parse', '--show-toplevel'], capture_output=True, text=True, check=True)
		repo_root = result.stdout.strip()
	except (subprocess.CalledProcessError, FileNotFoundError):
		repo_root = os.getcwd()
		if verbose:
			print(f"{Colors.YELLOW}Not in a git repository, using current directory{Colors.RESET}")

	print(f"Repository root: {Colors.BLUE}{repo_root}{Colors.RESET}")

	files = get_source_files(repo_root)

	if not files:
		print(f"{Colors.YELLOW}No source files found{Colors.RESET}")
		sys.exit(0)

	print(f"Found {Colors.BOLD}{len(files)}{Colors.RESET} source files")

	style_file = os.environ.get('CLANG_FORMAT_STYLE')

	if not style_file:
		style_file = find_file(
		    project_root=repo_root, script_name=".clang-format", env_var_name="_IGNORE_THIS_", max_depth=3)

	if style_file is None:
		print(f"{Colors.YELLOW}Warning: .clang-format file not found{Colors.RESET}", file=sys.stderr)
		print(f"{Colors.YELLOW}Using clang-format default style{Colors.RESET}", file=sys.stderr)
		style_arg = '--style=file'
	elif os.path.exists(str(style_file)):
		print(f"Using style file: {Colors.GREEN}{style_file}{Colors.RESET}")
		style_arg = f'--style=file:{style_file}'
	else:
		print(f"Using style: {Colors.GREEN}{style_file}{Colors.RESET}")
		style_arg = f'--style={style_file}'

	if mode == 'dry-run':
		print(f"\n{Colors.BOLD}Running in 'dry-run' mode{Colors.RESET}")
		needs_formatting, files_needing_format = check_needs_formatting(clang_format_cmd, files, style_arg, verbose)

		if needs_formatting:
			print(
			    f"\n{Colors.RED}{Emoji.CROSS} Found {len(files_needing_format)} "
			    f"file(s) that need formatting:{Colors.RESET}")
			for f in files_needing_format:
				print(f"  - {f}")
			sys.exit(1)
		else:
			print(f"\n{Colors.GREEN}{Emoji.CHECK} All files are properly formatted{Colors.RESET}")
			sys.exit(0)

	elif mode == 'pre-commit':
		print(f"\n{Colors.BOLD}Running in 'pre-commit' mode{Colors.RESET}")
		needs_formatting, files_needing_format = check_needs_formatting(clang_format_cmd, files, style_arg, verbose)

		if needs_formatting:
			print(
			    f"\n{Colors.YELLOW}{Emoji.WARNING} Found {len(files_needing_format)} "
			    f"file(s) that need formatting:{Colors.RESET}")
			for f in files_needing_format:
				print(f"  - {f}")
			print(f"\n{Colors.BOLD}Formatting files now...{Colors.RESET}")

			if not format_files(clang_format_cmd, files_needing_format, style_arg, in_place=True, verbose=verbose):
				print(f"\n{Colors.RED}{Emoji.CROSS} Error during formatting{Colors.RESET}", file=sys.stderr)
				sys.exit(1)

			print(f"\n{Colors.GREEN}{Emoji.CHECK} Files have been formatted{Colors.RESET}")
			print(
			    f"\n{Colors.YELLOW}{Emoji.WARNING} COMMIT ABORTED: Please review "
			    f"the formatting changes and commit again.{Colors.RESET}")
			sys.exit(1)
		else:
			print(f"\n{Colors.GREEN}{Emoji.CHECK} All files are properly formatted{Colors.RESET}")
			sys.exit(0)

	else:  # mode == 'format'
		print(f"\n{Colors.BOLD}Running in-place formatting mode{Colors.RESET}")
		print("Formatting all files...")

		if not format_files(clang_format_cmd, files, style_arg, in_place=True, verbose=verbose):
			print(f"\n{Colors.RED}{Emoji.CROSS} Error during formatting{Colors.RESET}", file=sys.stderr)
			sys.exit(1)

		print(f"\n{Colors.GREEN}{Emoji.CHECK} All files formatted successfully{Colors.RESET}")
		sys.exit(0)

if __name__ == '__main__':
	main()
