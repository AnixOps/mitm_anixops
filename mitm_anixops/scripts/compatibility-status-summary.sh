#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
MATRIX="$ROOT/docs/compatibility/matrix.md"

test -f "$MATRIX"

awk '
function trim(value) {
	gsub(/^[[:space:]]+/, "", value)
	gsub(/[[:space:]]+$/, "", value)
	return value
}

function fail(message) {
	print message > "/dev/stderr"
	failed = 1
	exit 1
}

BEGIN {
	allowed["supported"] = 1
	allowed["partial"] = 1
	allowed["planned"] = 1
	allowed["unsupported"] = 1
	expected_header = "| Capability | Ecosystem | Status | Source Contract | Parser Case | Positive Test | Negative Test | Compatibility Note | Unimplemented Items |"
}

$0 == expected_header {
	header_seen = 1
	next
}

/^\| --- / {
	next
}

/^\| / {
	if (!header_seen) {
		fail("compatibility matrix data row appears before header")
	}
	column_count = split($0, columns, "|")
	if (column_count != 11) {
		fail("matrix row has unexpected column count: " $0)
	}

	status = trim(columns[4])
	if (!(status in allowed)) {
		fail("matrix row has invalid status: " status)
	}

	counts[status]++
	total_count++
}

END {
	if (failed) {
		exit 1
	}
	if (!header_seen) {
		fail("compatibility matrix missing expected header")
	}
	if (total_count == 0) {
		fail("compatibility matrix has no capability rows")
	}
	printf "compatibility_supported_count=%d\n", counts["supported"] + 0
	printf "compatibility_partial_count=%d\n", counts["partial"] + 0
	printf "compatibility_planned_count=%d\n", counts["planned"] + 0
	printf "compatibility_unsupported_count=%d\n", counts["unsupported"] + 0
	printf "compatibility_total_count=%d\n", total_count
}
' "$MATRIX"
