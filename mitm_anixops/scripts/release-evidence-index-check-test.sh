#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

pass_output=$(mktemp "${TMPDIR:-/tmp}/mitm-release-evidence-index-pass.XXXXXX")
fail_output=$(mktemp "${TMPDIR:-/tmp}/mitm-release-evidence-index-fail.XXXXXX")
prerelease_output=$(mktemp "${TMPDIR:-/tmp}/mitm-release-evidence-index-prerelease.XXXXXX")
trap 'rm -f "$pass_output" "$fail_output" "$prerelease_output"' EXIT HUP INT TERM

sh "$ROOT/scripts/release-evidence-index-check.sh" v1.4.1 > "$pass_output"
grep -q "release evidence index check passed" "$pass_output"

if sh "$ROOT/scripts/release-evidence-index-check.sh" v1.4.2 > "$fail_output" 2>&1; then
	printf '%s\n' "release evidence index check should fail when latestStable is not the previous patch" >&2
	exit 1
fi
grep -q "latestStable must be v1.4.1 for release v1.4.2" "$fail_output"

sh "$ROOT/scripts/release-evidence-index-check.sh" v1.4.1-rc.1 > "$prerelease_output"
grep -q "release evidence index check passed" "$prerelease_output"

printf '%s\n' "release_evidence_index_check_test_status=passed"
