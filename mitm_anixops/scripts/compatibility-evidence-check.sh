#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DOC_DIR="$ROOT/docs/compatibility"
LEGACY_MATRIX="$ROOT/docs/compatibility_matrix.md"
FIXTURE_DIR="$ROOT/tests/fixtures"
TEST_CONFIG="$ROOT/tests/test_config.c"

test -d "$DOC_DIR"
test -f "$LEGACY_MATRIX"
test -d "$FIXTURE_DIR"
test -f "$TEST_CONFIG"

fixture_refs=$(mktemp)
config_refs=$(mktemp)
missing=$(mktemp)
trap 'rm -f "$fixture_refs" "$config_refs" "$missing"' EXIT HUP INT TERM

grep -Rho -E '`(tests/fixtures/)?[^`]+\.(conf|plugin|snippet|sgmodule|yaml|json|tsv|js)`' "$DOC_DIR" "$LEGACY_MATRIX" |
	tr -d '`' |
	sort -u > "$fixture_refs"

while IFS= read -r ref; do
	[ -n "$ref" ] || continue
	case "$ref" in
		tests/fixtures/*)
			path="$ROOT/$ref"
			;;
		*)
			path="$FIXTURE_DIR/$ref"
			;;
	esac
	if [ ! -f "$path" ]; then
		printf 'missing compatibility fixture reference: %s -> %s\n' "$ref" "$path" >> "$missing"
	fi
done < "$fixture_refs"

grep -Rho -E '`config/[^`]+`' "$DOC_DIR" "$LEGACY_MATRIX" |
	tr -d '`' |
	sort -u > "$config_refs"

while IFS= read -r test_id; do
	[ -n "$test_id" ] || continue
	if ! grep -F "\"$test_id\"" "$TEST_CONFIG" >/dev/null; then
		printf 'missing compatibility config test reference: %s\n' "$test_id" >> "$missing"
	fi
done < "$config_refs"

if [ -s "$missing" ]; then
	cat "$missing" >&2
	printf '%s\n' "compatibility evidence check failed" >&2
	exit 1
fi

fixture_count=$(wc -l < "$fixture_refs" | tr -d ' ')
config_count=$(wc -l < "$config_refs" | tr -d ' ')
printf 'compatibility evidence check passed (%s fixture refs, %s config test refs)\n' "$fixture_count" "$config_count"
