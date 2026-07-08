#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DOC_DIR="$ROOT/docs/compatibility"
LEGACY_MATRIX="$ROOT/docs/compatibility_matrix.md"
FIXTURE_DIR="$ROOT/tests/fixtures"
TEST_DIR="$ROOT/tests"

test -d "$DOC_DIR"
test -f "$LEGACY_MATRIX"
test -d "$FIXTURE_DIR"
test -d "$TEST_DIR"

fixture_refs=$(mktemp)
test_refs=$(mktemp)
root_fixture_files=$(mktemp)
missing=$(mktemp)
undocumented=$(mktemp)
trap 'rm -f "$fixture_refs" "$test_refs" "$root_fixture_files" "$missing" "$undocumented"' EXIT HUP INT TERM

{
	grep -Rho -E '`(tests/fixtures/)?[^`]+\.(conf|plugin|snippet|sgmodule|yaml|json|tsv|js)`' "$DOC_DIR" "$LEGACY_MATRIX"
	grep -Rho -E 'tests/fixtures/[A-Za-z0-9._/-]+\.(conf|plugin|snippet|sgmodule|yaml|json|tsv|js)' "$DOC_DIR" "$LEGACY_MATRIX"
} |
	tr -d '`' |
	sed 's#^tests/fixtures/##' |
	sort -u > "$fixture_refs"

while IFS= read -r ref; do
	[ -n "$ref" ] || continue
	path="$FIXTURE_DIR/$ref"
	if [ ! -f "$path" ]; then
		printf 'missing compatibility fixture reference: %s -> %s\n' "$ref" "$path" >> "$missing"
	fi
done < "$fixture_refs"

find "$FIXTURE_DIR" -maxdepth 1 -type f |
	while IFS= read -r path; do
		basename "$path"
	done |
	sort -u > "$root_fixture_files"

comm -23 "$root_fixture_files" "$fixture_refs" > "$undocumented"
if [ -s "$undocumented" ]; then
	while IFS= read -r fixture; do
		printf 'undocumented top-level compatibility fixture: %s\n' "$fixture" >> "$missing"
	done < "$undocumented"
fi

grep -Rho -E '`(config|script|rewrite|mitm|abi)/[A-Za-z0-9_][A-Za-z0-9_.-]*`' "$DOC_DIR" "$LEGACY_MATRIX" |
	tr -d '`' |
	sort -u > "$test_refs"

while IFS= read -r test_id; do
	[ -n "$test_id" ] || continue
	found=0
	for test_file in "$TEST_DIR"/test_*.c; do
		[ -f "$test_file" ] || continue
		if grep -F "\"$test_id\"" "$test_file" >/dev/null; then
			found=1
			break
		fi
	done
	if [ "$found" -ne 1 ]; then
		printf 'missing compatibility test reference: %s\n' "$test_id" >> "$missing"
	fi
done < "$test_refs"

if [ -s "$missing" ]; then
	cat "$missing" >&2
	printf '%s\n' "compatibility evidence check failed" >&2
	exit 1
fi

fixture_count=$(wc -l < "$fixture_refs" | tr -d ' ')
root_fixture_count=$(wc -l < "$root_fixture_files" | tr -d ' ')
test_count=$(wc -l < "$test_refs" | tr -d ' ')
printf 'compatibility evidence check passed (%s fixture refs, %s top-level fixtures, %s registered test refs)\n' "$fixture_count" "$root_fixture_count" "$test_count"
