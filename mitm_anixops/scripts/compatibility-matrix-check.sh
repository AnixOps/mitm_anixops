#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
MATRIX="$ROOT/docs/compatibility/matrix.md"
README="$ROOT/docs/compatibility/README.md"

test -f "$MATRIX"
test -f "$README"

grep -F "Every compatibility capability must have:" "$README" >/dev/null

required_capabilities='
Loon plugin common fields
Loon argument section
Loon script metadata
Loon task metadata
Quantumult X rewrite/task/mitm common config
Quantumult X task metadata
Surge module common config
Surge task metadata
Surge requirement metadata
Stash HTTP MITM hosts
hostname / MITM domain matching
request rewrite
response rewrite
header mutation
body mutation
script trigger
cron/task trigger
reject/direct/proxy policy intent
'

printf '%s\n' "$required_capabilities" | while IFS= read -r capability; do
	[ -n "$capability" ] || continue
	if ! grep -F "| $capability |" "$MATRIX" >/dev/null; then
		printf 'compatibility matrix missing required capability: %s\n' "$capability" >&2
		exit 1
	fi
done

awk -v root="$ROOT" '
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

function check_contract_paths(source, row, refs, ref, path, safe_path) {
	refs = source
	found = 0
	while (match(refs, /`[^`]+\.md(#[^`]*)?`/)) {
		ref = substr(refs, RSTART + 1, RLENGTH - 2)
		sub(/#.*/, "", ref)
		path = root "/docs/compatibility/" ref
		safe_path = path
		gsub(/"/, "\\\"", safe_path)
		if (system("test -f \"" safe_path "\"") != 0) {
			fail("matrix source contract does not exist for " row ": " ref)
		}
		found = 1
		refs = substr(refs, RSTART + RLENGTH)
	}
	if (!found) {
		fail("matrix row missing markdown source contract: " row)
	}
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

	capability = trim(columns[2])
	ecosystem = trim(columns[3])
	status = trim(columns[4])
	source = trim(columns[5])
	parser = trim(columns[6])
	positive = trim(columns[7])
	negative = trim(columns[8])
	note = trim(columns[9])
	unimplemented = trim(columns[10])

	if (capability == "" || ecosystem == "" || status == "" || source == "" ||
	    parser == "" || positive == "" || negative == "" || note == "" ||
	    unimplemented == "") {
		fail("matrix row has empty required evidence field: " $0)
	}
	if (!(status in allowed)) {
		fail("matrix row has invalid status for " capability ": " status)
	}
	if (positive ~ /^none; .*non-support evidence$/ && status != "unsupported") {
		fail("non-support guard row must use unsupported status: " capability)
	}
	check_contract_paths(source, capability)
	if (status == "partial" && (positive ~ /^none(;|$)/ || negative ~ /^none(;|$)/ ||
	    parser ~ /^none(;|$)/)) {
		fail("partial capability lacks concrete parser or test evidence: " capability)
	}
	if (status != "supported" && unimplemented ~ /^none(;|$)/) {
		fail("non-supported capability must list unimplemented items: " capability)
	}
	row_count++
}

END {
	if (failed) {
		exit 1
	}
	if (!header_seen) {
		fail("compatibility matrix missing expected header")
	}
	if (row_count < 10) {
		fail("compatibility matrix has too few capability rows")
	}
	printf "compatibility matrix check passed (%d rows)\n", row_count
}
' "$MATRIX"
