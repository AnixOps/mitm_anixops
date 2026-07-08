#!/bin/sh
set -eu

REPO_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$REPO_ROOT"

check_paths=".github mitm_anixops"

if git grep -n -I -E '[[:blank:]]$' -- $check_paths; then
	printf '%s\n' "static format check failed: trailing whitespace found" >&2
	exit 1
fi

carriage_return=$(printf '\r')
if git grep -n -I "$carriage_return" -- $check_paths; then
	printf '%s\n' "static format check failed: CRLF or carriage return found" >&2
	exit 1
fi

tmp_missing_newline=$(mktemp)
tmp_shell_scripts=$(mktemp)
trap 'rm -f "$tmp_missing_newline" "$tmp_shell_scripts"' EXIT HUP INT TERM

git ls-files -- $check_paths | while IFS= read -r file; do
	case "$file" in
		*.c|*.cmake|*.conf|*.gitignore|*.go|*.h|*.js|*.json|*.lock|*.md|*.pc|*.plugin|*.ps1|*.rs|*.sample|*.sgmodule|*.sh|*.snippet|*.toml|*.tsv|*.txt|*.yaml|*.yml|*/Makefile|Makefile)
			;;
		*)
			continue
			;;
	esac

	[ -s "$file" ] || continue
	if ! LC_ALL=C grep -Iq . "$file"; then
		continue
	fi
	if [ -n "$(tail -c 1 "$file")" ]; then
		printf '%s\n' "$file" >> "$tmp_missing_newline"
	fi
done

if [ -s "$tmp_missing_newline" ]; then
	cat "$tmp_missing_newline" >&2
	printf '%s\n' "static format check failed: text file missing final newline" >&2
	exit 1
fi

git ls-files -- '*.sh' > "$tmp_shell_scripts"
while IFS= read -r script; do
	sh -n "$script"
done < "$tmp_shell_scripts"

printf '%s\n' "static format check passed"
