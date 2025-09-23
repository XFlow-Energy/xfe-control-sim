#!/bin/bash
set -x
toplevel="$(git rev-parse --show-toplevel)"

# choose flags based on CI mode
if [ "$1" = "dry-run" ]; then
	FLAGS="--verbose --dry-run --Werror"
else
	FLAGS="--verbose -i"
fi

echo "Descending from $toplevel"

find "$toplevel" -path "$toplevel/build" -prune -o \( -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' \) -exec clang-format $FLAGS "--style=file:$toplevel/.clang-format" {} +