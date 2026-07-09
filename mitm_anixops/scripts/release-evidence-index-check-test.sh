#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

pass_output=$(mktemp "${TMPDIR:-/tmp}/mitm-release-evidence-index-pass.XXXXXX")
fail_output=$(mktemp "${TMPDIR:-/tmp}/mitm-release-evidence-index-fail.XXXXXX")
prerelease_output=$(mktemp "${TMPDIR:-/tmp}/mitm-release-evidence-index-prerelease.XXXXXX")
unsorted_index=$(mktemp "${TMPDIR:-/tmp}/mitm-release-evidence-index-unsorted.XXXXXX")
unsorted_output=$(mktemp "${TMPDIR:-/tmp}/mitm-release-evidence-index-unsorted-fail.XXXXXX")
gap_index=$(mktemp "${TMPDIR:-/tmp}/mitm-release-evidence-index-gap.XXXXXX")
gap_output=$(mktemp "${TMPDIR:-/tmp}/mitm-release-evidence-index-gap-fail.XXXXXX")
trap 'rm -f "$pass_output" "$fail_output" "$prerelease_output" "$unsorted_index" "$unsorted_output" "$gap_index" "$gap_output"' EXIT HUP INT TERM

sh "$ROOT/scripts/release-evidence-index-check.sh" v1.4.5 > "$pass_output"
grep -q "release evidence index check passed" "$pass_output"

if sh "$ROOT/scripts/release-evidence-index-check.sh" v1.4.6 > "$fail_output" 2>&1; then
	printf '%s\n' "release evidence index check should fail when latestStable is not the previous patch" >&2
	exit 1
fi
grep -q "latestStable must be v1.4.5 for release v1.4.6" "$fail_output"

sh "$ROOT/scripts/release-evidence-index-check.sh" v1.4.5-rc.1 > "$prerelease_output"
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

index.entries[2] = {
  ...index.entries[2],
  version: "v1.4.5",
  releaseUrl: "https://github.com/AnixOps/mitm_anixops/releases/tag/v1.4.5",
};
fs.writeFileSync(outputPath, `${JSON.stringify(index, null, 2)}\n`);
NODE

if RELEASE_EVIDENCE_INDEX_PATH="$unsorted_index" sh "$ROOT/scripts/release-evidence-index-check.sh" v1.4.5 > "$unsorted_output" 2>&1; then
	printf '%s\n' "release evidence index check should fail when entries are not newest-to-oldest" >&2
	exit 1
fi
grep -q "release evidence entries must be sorted newest-to-oldest" "$unsorted_output"

node - "$ROOT/docs/release_evidence_index.json" "$gap_index" <<'NODE'
const fs = require("fs");
const inputPath = process.argv[2];
const outputPath = process.argv[3];
const index = JSON.parse(fs.readFileSync(inputPath, "utf8"));

if (!Array.isArray(index.entries) || index.entries.length < 3) {
  console.error("release evidence index test needs at least three entries");
  process.exit(1);
}

index.entries[1] = {
  ...index.entries[2],
  version: "v1.4.2",
  releaseUrl: "https://github.com/AnixOps/mitm_anixops/releases/tag/v1.4.2",
};
index.entries[2] = {
  ...index.entries[2],
  version: "v1.4.1",
  releaseUrl: "https://github.com/AnixOps/mitm_anixops/releases/tag/v1.4.1",
};
fs.writeFileSync(outputPath, `${JSON.stringify(index, null, 2)}\n`);
NODE

if RELEASE_EVIDENCE_INDEX_PATH="$gap_index" sh "$ROOT/scripts/release-evidence-index-check.sh" v1.4.5 > "$gap_output" 2>&1; then
	printf '%s\n' "release evidence index check should fail when retained patch entries skip a version" >&2
	exit 1
fi
grep -q "release evidence entries must be contiguous newest-to-oldest" "$gap_output"

printf '%s\n' "release_evidence_index_check_test_status=passed"
