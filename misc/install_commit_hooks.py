#!/usr/bin/env python3
"""
Install Git hooks for the repository.

This script copies pre-commit.py and other hooks from the repository
to the .git/hooks directory, making them active.
"""

import os
import shutil
import sys
from typing import Optional, Tuple, List

from xflow_shared_functions import Colors, Emoji, get_git_root, setup_display

# Initialize display settings
setup_display()

def find_hooks_directory(git_root: str) -> Optional[str]:
	"""
    Find the directory containing hook scripts.
    
    Searches in order:
    1. GIT_HOOKS_SOURCE environment variable
    2. Common locations within repository
    
    Returns:
        Path to hooks directory or None if not found.
    """
	if 'GIT_HOOKS_SOURCE' in os.environ:
		hooks_dir = os.environ['GIT_HOOKS_SOURCE']
		if os.path.exists(hooks_dir):
			return hooks_dir

	search_paths = [
	    'hooks',
	    'git-hooks',
	    '.githooks',
	    'scripts/hooks',
	    'tools/hooks',
	    'c/src/misc',
	    'misc',
	]

	for rel_path in search_paths:
		full_path = os.path.join(git_root, rel_path)
		pre_commit = os.path.join(full_path, 'pre-commit.py')
		if os.path.exists(pre_commit):
			return full_path

	return None

def install_hook(source_path: str, dest_path: str, verbose: bool = False) -> bool:
	"""
    Install a single hook file.
    
    Args:
        source_path: Source file path.
        dest_path: Destination file path.
        verbose: Print detailed progress.
    
    Returns:
        True if successful, False otherwise.
    """
	try:
		if os.name == 'nt' and source_path.endswith('.py'):
			# On Windows with Git Bash, create a shell wrapper
			# Find Python executable
			python_exe = shutil.which('python') or shutil.which('python3') or sys.executable

			# Create a bash wrapper that Git Bash can execute
			wrapper_content = f'''#!/bin/sh
"{python_exe}" "{source_path}" "$@"
'''
			with open(dest_path, 'w', newline='\n') as f:  # Force Unix line endings
				f.write(wrapper_content)

			if verbose:
				print(f"{Colors.GREEN} {Colors.RESET} Installed (Git Bash wrapper): {os.path.basename(dest_path)}")
		else:
			# Unix: just copy and make executable
			shutil.copy2(source_path, dest_path)
			if os.name != 'nt':
				os.chmod(dest_path, 0o755)

			if verbose:
				print(f"{Colors.GREEN} {Colors.RESET} Installed: {os.path.basename(dest_path)}")

		return True
	except (IOError, OSError) as e:
		print(
		    f"{Colors.RED} {Colors.RESET} Failed to install "
		    f"{os.path.basename(source_path)}: {e}", file=sys.stderr)
		return False

def main():
	"""Main entry point."""
	verbose = '-v' in sys.argv or '--verbose' in sys.argv
	force = '-f' in sys.argv or '--force' in sys.argv

	if '-h' in sys.argv or '--help' in sys.argv:
		print(
		    f"""
{Colors.BOLD}Usage:{Colors.RESET} {sys.argv[0]} [options]

Install Git hooks from the repository to .git/hooks directory.

{Colors.BOLD}Options:{Colors.RESET}
  -v, --verbose   Show detailed progress
  -f, --force     Overwrite existing hooks without prompting
  -h, --help      Show this help message

{Colors.BOLD}Environment Variables:{Colors.RESET}
  GIT_HOOKS_SOURCE    Directory containing hook scripts

{Colors.BOLD}Examples:{Colors.RESET}
  {sys.argv[0]}              # Install hooks with prompts
  {sys.argv[0]} --force      # Install hooks, overwriting existing
  {sys.argv[0]} --verbose    # Show detailed progress

{Colors.BOLD}Note:{Colors.RESET}
  Hooks with .py extension will be copied without the extension
  (e.g., pre-commit.py -> pre-commit)
        """)
		sys.exit(0)

	git_root = get_git_root()
	hooks_target_dir = git_root / '.git' / 'hooks'

	hooks_source_dir = find_hooks_directory(str(git_root))

	if not hooks_source_dir:
		print(f"{Colors.RED}Error: Could not find hooks directory in repository{Colors.RESET}", file=sys.stderr)
		print(f"\n{Colors.YELLOW}Searched in:{Colors.RESET}", file=sys.stderr)
		print(f"  - hooks/, git-hooks/, .githooks/", file=sys.stderr)
		print(f"  - scripts/hooks/, tools/hooks/", file=sys.stderr)
		print(f"  - c/src/misc/, misc/", file=sys.stderr)
		print(f"\n{Colors.YELLOW}Tip: Set GIT_HOOKS_SOURCE environment variable{Colors.RESET}", file=sys.stderr)
		sys.exit(1)

	if not os.path.exists(hooks_target_dir):
		print(f"{Colors.RED}Error: .git/hooks directory not found{Colors.RESET}", file=sys.stderr)
		print(f"Are you in a git repository?", file=sys.stderr)
		sys.exit(1)

	print(f"{Colors.BOLD}Installing Git Hooks{Colors.RESET}")
	print(f"Repository: {Colors.BLUE}{git_root}{Colors.RESET}")
	print(f"Source: {Colors.BLUE}{os.path.relpath(hooks_source_dir, git_root)}{Colors.RESET}")
	print(f"Target: {Colors.BLUE}.git/hooks{Colors.RESET}")
	print()

	# Define hooks: (source_filename, destination_hook_name)
	hook_definitions = [
	    ('pre-commit.py', 'pre-commit'),
	    ('pre-push', 'pre-push'),
	    ('post-commit', 'post-commit'),
	    ('prepare-commit-msg', 'prepare-commit-msg'),
	    ('commit-msg', 'commit-msg'),
	]

	hooks_to_install: List[Tuple[str, str, str]] = []
	for source_name, dest_name in hook_definitions:
		source_path = os.path.join(hooks_source_dir, source_name)
		if os.path.exists(source_path):
			hooks_to_install.append((source_name, dest_name, source_path))

	if not hooks_to_install:
		print(f"{Colors.YELLOW}No hook files found in {hooks_source_dir}{Colors.RESET}")
		sys.exit(0)

	print(f"Found {Colors.BOLD}{len(hooks_to_install)}{Colors.RESET} hook(s) to install:")
	for source_name, dest_name, _ in hooks_to_install:
		if source_name != dest_name:
			print(f"  - {source_name} -> {dest_name}")
		else:
			print(f"  - {dest_name}")
	print()

	existing_hooks = []
	for _, dest_name, _ in hooks_to_install:
		dest_path = os.path.join(hooks_target_dir, dest_name)
		if os.path.exists(dest_path):
			existing_hooks.append(dest_name)

	if existing_hooks and not force:
		print(f"{Colors.YELLOW}The following hooks already exist:{Colors.RESET}")
		for hook_name in existing_hooks:
			print(f"  - {hook_name}")
		print()

		response = input(f"Overwrite existing hooks? [y/N]: ").strip().lower()
		if response not in ['y', 'yes']:
			print("Installation cancelled.")
			sys.exit(0)

	success_count = 0
	for source_name, dest_name, source_path in hooks_to_install:
		dest_path = os.path.join(hooks_target_dir, dest_name)

		if install_hook(source_path, dest_path, verbose):
			success_count += 1

	print()
	if success_count == len(hooks_to_install):
		print(f"{Colors.GREEN}{Emoji.CHECK} Successfully installed "
		      f"{success_count} hook(s){Colors.RESET}")
	else:
		print(
		    f"{Colors.YELLOW}{Emoji.WARNING} Installed {success_count}/"
		    f"{len(hooks_to_install)} hook(s){Colors.RESET}")
		sys.exit(1)

	print()
	print(f"{Colors.BOLD}Hooks are now active!{Colors.RESET}")
	print(f"Pre-commit checks will run automatically before each commit.")

if __name__ == '__main__':
	main()
