<#
.SYNOPSIS
    Recursively finds and formats C/C++ source and header files using clang-format.
.DESCRIPTION
    This script starts from the git repository root, finds all files with extensions
    .c, .h, .cpp, and .hpp, excluding the 'build' directory. It then runs
    clang-format on these files to format them in-place according to the
    style file specified.
#>

# --- Script Initialization ---

# 1. Find the root of the git repository.
try {
    $toplevel = git rev-parse --show-toplevel
    Write-Host "Descending from $toplevel"
}
catch {
    Write-Error "Failed to find git repository root. This script must be run from within a git repository."
    exit 1
}

# 2. Locate the clang-format executable.
$clangFormatPath = Get-Command clang-format -ErrorAction SilentlyContinue
if (-not $clangFormatPath) {
    Write-Error "clang-format not found in your PATH. Please install LLVM/clang-tools or add it to your PATH."
    exit 1
}
# Get the full path to the executable.
$clangFormatPath = $clangFormatPath.Source

# --- Main Logic ---

# 3. Define the path to the build directory to exclude it from formatting.
$buildDirectoryPath = Join-Path $toplevel "build"

# 4. Construct the argument for the .clang-format style file.
$styleFilePath = Join-Path $toplevel '.clang-format'
$styleArgument = "--style=file:$styleFilePath"

# 5. Find all target files, filtering out any that are in the build directory.
$filesToFormat = Get-ChildItem -Path $toplevel -Recurse -Include *.c, *.h, *.cpp, *.hpp | Where-Object {
    $_.FullName -notlike "$buildDirectoryPath\*"
} | Select-Object -ExpandProperty FullName

if ($filesToFormat.Count -eq 0) {
    Write-Host "No C/C++ files found to format."
    exit 0
}

# 6. Execute clang-format on the list of found files.
Write-Host "Formatting $($filesToFormat.Count) files..."
& $clangFormatPath --verbose -i $styleArgument $filesToFormat

Write-Host "Formatting complete."
