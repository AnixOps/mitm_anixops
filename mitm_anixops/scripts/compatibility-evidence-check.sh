#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DOC_DIR="$ROOT/docs/compatibility"
LEGACY_MATRIX="$ROOT/docs/compatibility_matrix.md"
FIXTURE_DIR="$ROOT/tests/fixtures"
TEST_DIR="$ROOT/tests"
MAKEFILE="$ROOT/Makefile"
GO_BINDING_DIR="$ROOT/bindings/go"
RUST_BINDING_DIR="$ROOT/bindings/rust"

test -d "$DOC_DIR"
test -f "$LEGACY_MATRIX"
test -d "$FIXTURE_DIR"
test -d "$TEST_DIR"
test -f "$MAKEFILE"
test -d "$GO_BINDING_DIR"
test -d "$RUST_BINDING_DIR"

fixture_refs=$(mktemp)
test_refs=$(mktemp)
make_refs=$(mktemp)
script_refs=$(mktemp)
go_test_refs=$(mktemp)
rust_test_refs=$(mktemp)
root_fixture_files=$(mktemp)
missing=$(mktemp)
undocumented=$(mktemp)
trap 'rm -f "$fixture_refs" "$test_refs" "$make_refs" "$script_refs" "$go_test_refs" "$rust_test_refs" "$root_fixture_files" "$missing" "$undocumented"' EXIT HUP INT TERM

make_target_exists() {
	target=$1
	awk -v target="$target" '
		/^[A-Za-z0-9_.-]+:/ {
			name = $1
			sub(/:.*/, "", name)
			if (name == target) {
				found = 1
			}
		}
		END {
			exit found ? 0 : 1
		}
	' "$MAKEFILE"
}

go_test_exists() {
	test_name=$1
	grep -R -E "func[[:space:]]+$test_name[[:space:]]*\\(" "$GO_BINDING_DIR" >/dev/null 2>&1
}

rust_test_exists() {
	test_name=$1
	grep -R -E "fn[[:space:]]+$test_name[[:space:]]*\\(" "$RUST_BINDING_DIR" >/dev/null 2>&1
}

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

grep -Rho -E '`make [A-Za-z0-9_.-]+`' "$DOC_DIR" "$LEGACY_MATRIX" |
	sed 's/^`make //; s/`$//' |
	sort -u > "$make_refs"

while IFS= read -r make_target; do
	[ -n "$make_target" ] || continue
	if ! make_target_exists "$make_target"; then
		printf 'missing compatibility Make target reference: make %s\n' "$make_target" >> "$missing"
	fi
done < "$make_refs"

grep -Rho -E '`(e2e/scripts|scripts)/[A-Za-z0-9._/-]+\.sh`' "$DOC_DIR" "$LEGACY_MATRIX" |
	tr -d '`' |
	sort -u > "$script_refs"

while IFS= read -r script_ref; do
	[ -n "$script_ref" ] || continue
	path="$ROOT/$script_ref"
	if [ ! -f "$path" ]; then
		printf 'missing compatibility script reference: %s -> %s\n' "$script_ref" "$path" >> "$missing"
	fi
done < "$script_refs"

grep -Rho -E '`Test[A-Za-z0-9_]+`' "$DOC_DIR" "$LEGACY_MATRIX" |
	tr -d '`' |
	sort -u > "$go_test_refs"

while IFS= read -r go_test; do
	[ -n "$go_test" ] || continue
	if ! go_test_exists "$go_test"; then
		printf 'missing compatibility Go test reference: %s\n' "$go_test" >> "$missing"
	fi
done < "$go_test_refs"

grep -Rho -E '`rust_[A-Za-z0-9_]+`' "$DOC_DIR" "$LEGACY_MATRIX" |
	tr -d '`' |
	sort -u > "$rust_test_refs"

while IFS= read -r rust_test; do
	[ -n "$rust_test" ] || continue
	if ! rust_test_exists "$rust_test"; then
		printf 'missing compatibility Rust test reference: %s\n' "$rust_test" >> "$missing"
	fi
done < "$rust_test_refs"

if [ -s "$missing" ]; then
	cat "$missing" >&2
	printf '%s\n' "compatibility evidence check failed" >&2
	exit 1
fi

fixture_count=$(wc -l < "$fixture_refs" | tr -d ' ')
root_fixture_count=$(wc -l < "$root_fixture_files" | tr -d ' ')
test_count=$(wc -l < "$test_refs" | tr -d ' ')
make_count=$(wc -l < "$make_refs" | tr -d ' ')
script_count=$(wc -l < "$script_refs" | tr -d ' ')
go_test_count=$(wc -l < "$go_test_refs" | tr -d ' ')
rust_test_count=$(wc -l < "$rust_test_refs" | tr -d ' ')
printf 'compatibility evidence check passed (%s fixture refs, %s top-level fixtures, %s registered C test refs, %s Make target refs, %s script refs, %s Go test refs, %s Rust test refs)\n' "$fixture_count" "$root_fixture_count" "$test_count" "$make_count" "$script_count" "$go_test_count" "$rust_test_count"
