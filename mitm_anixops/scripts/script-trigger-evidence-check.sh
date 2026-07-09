#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SCRIPT_TEST_FILE=${SCRIPT_TRIGGER_TEST_FILE:-"$ROOT/tests/test_script.c"}
MATRIX=${SCRIPT_TRIGGER_MATRIX_FILE:-"$ROOT/docs/compatibility/matrix.md"}
SOURCE_CONTRACTS=${SCRIPT_TRIGGER_SOURCE_CONTRACTS_FILE:-"$ROOT/docs/compatibility/source-contracts.md"}

test -f "$SCRIPT_TEST_FILE"
test -f "$MATRIX"
test -f "$SOURCE_CONTRACTS"

script_tests=$(mktemp)
missing=$(mktemp)
trap 'rm -f "$script_tests" "$missing"' EXIT HUP INT TERM

grep -Eo '"script/[A-Za-z0-9_][A-Za-z0-9_.-]*"' "$SCRIPT_TEST_FILE" |
	tr -d '"' |
	sort -u > "$script_tests"

if [ ! -s "$script_tests" ]; then
	printf '%s\n' "script trigger evidence check failed: no registered script tests found" >&2
	exit 1
fi

script_trigger_row=$(grep -F '| script trigger |' "$MATRIX" || true)
if [ -z "$script_trigger_row" ]; then
	printf '%s\n' "script trigger evidence check failed: matrix missing script trigger row" >&2
	exit 1
fi

while IFS= read -r test_id; do
	[ -n "$test_id" ] || continue
	if ! printf '%s\n' "$script_trigger_row" | grep -F "\`$test_id\`" >/dev/null; then
		printf 'missing script trigger matrix evidence: %s\n' "$test_id" >> "$missing"
	fi
	if ! grep -F "\`$test_id\`" "$SOURCE_CONTRACTS" >/dev/null; then
		printf 'missing script trigger source-contract evidence: %s\n' "$test_id" >> "$missing"
	fi
done < "$script_tests"

if [ -s "$missing" ]; then
	cat "$missing" >&2
	printf '%s\n' "script trigger evidence check failed" >&2
	exit 1
fi

test_count=$(wc -l < "$script_tests" | tr -d ' ')
printf 'script trigger evidence check passed (%s registered script tests)\n' "$test_count"
