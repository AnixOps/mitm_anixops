#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CHECK="$ROOT/scripts/script-trigger-evidence-check.sh"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

TEST_FILE="$TMP_DIR/test_script.c"
MATRIX_FILE="$TMP_DIR/matrix.md"
SOURCE_FILE="$TMP_DIR/source-contracts.md"
OUTPUT_FILE="$TMP_DIR/output.txt"

write_test_file() {
	test_id=$1
	cat > "$TEST_FILE" <<EOF
void register_tests(void) {
	add_test(tests, count, cap, "$test_id", test_fn);
}
EOF
}

write_matrix_file() {
	evidence=$1
	cat > "$MATRIX_FILE" <<EOF
| Capability | Ecosystem | Status | Source Contract | Parser Case | Positive Test | Negative Test | Compatibility Note | Unimplemented Items |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| script trigger | Loon, Surge, QX | partial | \`source-contracts.md#script-trigger-metadata\` | representative script fixtures | $evidence | \`script/negative_guard\` | Dispatch metadata and Alpha Node runner | production JS runtime |
EOF
}

write_source_file() {
	evidence=$1
	cat > "$SOURCE_FILE" <<EOF
# Source Contracts

Current CI evidence:

- $evidence;
EOF
}

write_test_file "script/example_trigger"
write_matrix_file "\`script/example_trigger\`"
write_source_file "\`script/example_trigger\`"
SCRIPT_TRIGGER_TEST_FILE="$TEST_FILE" \
	SCRIPT_TRIGGER_MATRIX_FILE="$MATRIX_FILE" \
	SCRIPT_TRIGGER_SOURCE_CONTRACTS_FILE="$SOURCE_FILE" \
	sh "$CHECK" > "$OUTPUT_FILE"
grep -q "script trigger evidence check passed (1 registered script tests)" "$OUTPUT_FILE"

write_test_file "script/missing_matrix"
write_matrix_file "\`script/other_trigger\`"
write_source_file "\`script/missing_matrix\`"
if SCRIPT_TRIGGER_TEST_FILE="$TEST_FILE" \
	SCRIPT_TRIGGER_MATRIX_FILE="$MATRIX_FILE" \
	SCRIPT_TRIGGER_SOURCE_CONTRACTS_FILE="$SOURCE_FILE" \
	sh "$CHECK" > "$OUTPUT_FILE" 2>&1; then
	printf '%s\n' "script trigger evidence check test failed: missing matrix evidence was accepted" >&2
	exit 1
fi
grep -q "missing script trigger matrix evidence: script/missing_matrix" "$OUTPUT_FILE"

write_test_file "script/missing_source_contract"
write_matrix_file "\`script/missing_source_contract\`"
write_source_file "\`script/other_trigger\`"
if SCRIPT_TRIGGER_TEST_FILE="$TEST_FILE" \
	SCRIPT_TRIGGER_MATRIX_FILE="$MATRIX_FILE" \
	SCRIPT_TRIGGER_SOURCE_CONTRACTS_FILE="$SOURCE_FILE" \
	sh "$CHECK" > "$OUTPUT_FILE" 2>&1; then
	printf '%s\n' "script trigger evidence check test failed: missing source evidence was accepted" >&2
	exit 1
fi
grep -q "missing script trigger source-contract evidence: script/missing_source_contract" "$OUTPUT_FILE"

printf '%s\n' "script trigger evidence check test passed"
