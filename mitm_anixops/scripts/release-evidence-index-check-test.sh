#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

pass_output=$(mktemp "${TMPDIR:-/tmp}/mitm-release-evidence-index-pass.XXXXXX")
fail_output=$(mktemp "${TMPDIR:-/tmp}/mitm-release-evidence-index-fail.XXXXXX")
prerelease_output=$(mktemp "${TMPDIR:-/tmp}/mitm-release-evidence-index-prerelease.XXXXXX")
unsorted_index=$(mktemp "${TMPDIR:-/tmp}/mitm-release-evidence-index-unsorted.XXXXXX")
unsorted_output=$(mktemp "${TMPDIR:-/tmp}/mitm-release-evidence-index-unsorted-fail.XXXXXX")
trap 'rm -f "$pass_output" "$fail_output" "$prerelease_output" "$unsorted_index" "$unsorted_output"' EXIT HUP INT TERM

sh "$ROOT/scripts/release-evidence-index-check.sh" v1.4.4 > "$pass_output"
grep -q "release evidence index check passed" "$pass_output"

if sh "$ROOT/scripts/release-evidence-index-check.sh" v1.4.5 > "$fail_output" 2>&1; then
	printf '%s\n' "release evidence index check should fail when latestStable is not the previous patch" >&2
	exit 1
fi
grep -q "latestStable must be v1.4.4 for release v1.4.5" "$fail_output"

sh "$ROOT/scripts/release-evidence-index-check.sh" v1.4.4-rc.1 > "$prerelease_output"
grep -q "release evidence index check passed" "$prerelease_output"

node - "$ROOT/docs/release_evidence_index.json" "$unsorted_index" <<'NODE'
const fs = require("fs");
const inputPath = process.argv[2];
const outputPath = process.argv[3];
const index = JSON.parse(fs.readFileSync(inputPath, "utf8"));

if (!Array.isArray(index.entries) || index.entries.length < 3) {
  console.error("release evidence index test needs at least three entries");
  process.exit(1);
}

const secondEntry = index.entries[1];
index.entries[1] = index.entries[2];
index.entries[2] = secondEntry;
fs.writeFileSync(outputPath, `${JSON.stringify(index, null, 2)}\n`);
NODE

if RELEASE_EVIDENCE_INDEX_PATH="$unsorted_index" sh "$ROOT/scripts/release-evidence-index-check.sh" v1.4.4 > "$unsorted_output" 2>&1; then
	printf '%s\n' "release evidence index check should fail when entries are not newest-to-oldest" >&2
	exit 1
fi
grep -q "release evidence entries must be sorted newest-to-oldest" "$unsorted_output"

printf '%s\n' "release_evidence_index_check_test_status=passed"
