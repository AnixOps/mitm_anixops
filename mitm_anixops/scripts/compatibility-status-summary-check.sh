#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SUMMARY="$ROOT/scripts/compatibility-status-summary.sh"

test -f "$SUMMARY"

summary_output=$(mktemp)
trap 'rm -f "$summary_output"' EXIT HUP INT TERM

sh "$SUMMARY" > "$summary_output"

awk '
function fail(message) {
	print message > "/dev/stderr"
	failed = 1
	exit 1
}

BEGIN {
	expected_order = "compatibility_supported_count compatibility_partial_count compatibility_planned_count compatibility_unsupported_count compatibility_total_count"
	expected_count = split(expected_order, expected_keys, " ")
	for (i = 1; i <= expected_count; i++) {
		expected[expected_keys[i]] = 1
	}
}

{
	part_count = split($0, parts, "=")
	if (part_count != 2) {
		fail("compatibility status summary has malformed line: " $0)
	}

	key = parts[1]
	value = parts[2]
	if (!(key in expected)) {
		fail("compatibility status summary has unexpected key: " key)
	}
	if (value !~ /^[0-9]+$/) {
		fail("compatibility status summary value is not a non-negative integer: " key "=" value)
	}

	seen[key]++
	values[key] = value + 0
	row_count++
}

END {
	if (failed) {
		exit 1
	}
	for (i = 1; i <= expected_count; i++) {
		key = expected_keys[i]
		if (seen[key] != 1) {
			fail("compatibility status summary key must appear exactly once: " key)
		}
	}
	if (row_count != expected_count) {
		fail("compatibility status summary must contain exactly 5 lines")
	}

	total = values["compatibility_supported_count"] + values["compatibility_partial_count"] + values["compatibility_planned_count"] + values["compatibility_unsupported_count"]
	if (total != values["compatibility_total_count"]) {
		fail("compatibility status summary total does not match status counts")
	}
	if (values["compatibility_total_count"] == 0) {
		fail("compatibility status summary total must be greater than zero")
	}

	printf "compatibility status summary check passed (supported=%d partial=%d planned=%d unsupported=%d total=%d)\n", values["compatibility_supported_count"], values["compatibility_partial_count"], values["compatibility_planned_count"], values["compatibility_unsupported_count"], values["compatibility_total_count"]
}
' "$summary_output"
