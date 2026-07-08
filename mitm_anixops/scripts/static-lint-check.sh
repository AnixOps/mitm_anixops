#!/bin/sh
set -eu

REPO_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$REPO_ROOT"

if ! command -v shellcheck >/dev/null 2>&1; then
	printf '%s\n' "static lint check failed: shellcheck is required" >&2
	exit 1
fi

tmp_shell_scripts=$(mktemp)
trap 'rm -f "$tmp_shell_scripts"' EXIT HUP INT TERM

git ls-files -- '*.sh' > "$tmp_shell_scripts"
if [ ! -s "$tmp_shell_scripts" ]; then
	printf '%s\n' "static lint check failed: no shell scripts found" >&2
	exit 1
fi

while IFS= read -r script; do
	shellcheck -S error "$script"
done < "$tmp_shell_scripts"

printf '%s\n' "static lint check passed"
