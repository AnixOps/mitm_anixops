#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
REPO=$(CDPATH= cd -- "$ROOT/.." && pwd)

INDEX="$ROOT/docs/release_evidence_index.json"
RELEASE_GATE="$ROOT/docs/architecture/release-gate.md"
RELEASE_CHECKLIST="$ROOT/docs/release_checklist.md"
CHECK_SCRIPT="$ROOT/scripts/check.sh"
V1_ACCEPTANCE="$ROOT/scripts/v1-acceptance-check.sh"
BUILD_WORKFLOW="$REPO/.github/workflows/build.yml"
DRY_RUN_WORKFLOW="$REPO/.github/workflows/release-dry-run.yml"
RELEASE_WORKFLOW="$REPO/.github/workflows/release.yml"

for file in \
	"$INDEX" \
	"$RELEASE_GATE" \
	"$RELEASE_CHECKLIST" \
	"$CHECK_SCRIPT" \
	"$V1_ACCEPTANCE" \
	"$BUILD_WORKFLOW" \
	"$DRY_RUN_WORKFLOW" \
	"$RELEASE_WORKFLOW"
do
	test -s "$file"
done

require_command() {
	if ! command -v "$1" >/dev/null 2>&1; then
		printf 'missing required command: %s\n' "$1" >&2
		exit 1
	fi
}

require_pattern() {
	file=$1
	pattern=$2
	message=$3

	if ! grep -F -- "$pattern" "$file" >/dev/null; then
		printf '%s: %s\n' "$message" "$pattern" >&2
		exit 1
	fi
}

require_command node

node - "$INDEX" <<'NODE'
const fs = require("fs");
const path = process.argv[2];
const index = JSON.parse(fs.readFileSync(path, "utf8"));

function fail(message) {
  console.error(message);
  process.exit(1);
}

function assert(condition, message) {
  if (!condition) {
    fail(message);
  }
}

const expectedLatestStable = {
  version: "v1.3.3",
  targetCommit: "5bf7ee5b791a5982361bca4a29624d3a79714853",
  ciRunId: "29039127942",
  releaseWorkflowRunId: "29039516986",
  publicationEvidenceArtifact: "anixops-mitm-release-publication-evidence",
  publicationEvidenceFile: "release-publication-verify.env",
};

assert(index.schemaVersion === "release-evidence-index-v1", "invalid release evidence index schemaVersion");
assert(index.latestStable === expectedLatestStable.version, `latestStable must be ${expectedLatestStable.version}`);
assert(Array.isArray(index.entries), "entries must be an array");
assert(index.entries.length >= 2, "entries must include latest and historical release evidence");
assert(index.entries[0].version === index.latestStable, "first release evidence entry must match latestStable");

const versions = new Set();
for (const entry of index.entries) {
  assert(/^v[0-9]+\.[0-9]+\.[0-9]+$/.test(entry.version), `invalid version: ${entry.version}`);
  assert(!versions.has(entry.version), `duplicate release evidence entry: ${entry.version}`);
  versions.add(entry.version);
  assert(entry.releaseUrl === `https://github.com/AnixOps/mitm_anixops/releases/tag/${entry.version}`, `invalid releaseUrl for ${entry.version}`);
  assert(/^[0-9a-f]{40}$/.test(entry.targetCommit), `invalid targetCommit for ${entry.version}`);
  assert(/^[0-9]+$/.test(entry.ciRunId), `invalid ciRunId for ${entry.version}`);
  assert(entry.ciRunUrl === `https://github.com/AnixOps/mitm_anixops/actions/runs/${entry.ciRunId}`, `invalid ciRunUrl for ${entry.version}`);
  assert(/^[0-9]+$/.test(entry.releaseWorkflowRunId), `invalid releaseWorkflowRunId for ${entry.version}`);
  assert(entry.releaseWorkflowRunUrl === `https://github.com/AnixOps/mitm_anixops/actions/runs/${entry.releaseWorkflowRunId}`, `invalid releaseWorkflowRunUrl for ${entry.version}`);
  assert(entry.assetCount === 7, `assetCount must be 7 for ${entry.version}`);
  assert(entry.artifactCount === 2, `artifactCount must be 2 for ${entry.version}`);
  assert(Array.isArray(entry.artifactPlatforms), `artifactPlatforms must be an array for ${entry.version}`);
  assert(entry.artifactPlatforms.join(",") === "linux-x64,windows-x64", `invalid artifactPlatforms for ${entry.version}`);
  assert(entry.publicationVerifyStatus === "passed", `publication verifier must pass for ${entry.version}`);
}

const latest = index.entries.find((entry) => entry.version === index.latestStable);
assert(latest, "missing latest stable release evidence entry");
assert(versions.has("v1.3.2"), "missing retained v1.3.2 release evidence entry");
assert(latest.targetCommit === expectedLatestStable.targetCommit, `${expectedLatestStable.version} targetCommit mismatch`);
assert(latest.ciRunId === expectedLatestStable.ciRunId, `${expectedLatestStable.version} ciRunId mismatch`);
assert(latest.releaseWorkflowRunId === expectedLatestStable.releaseWorkflowRunId, `${expectedLatestStable.version} releaseWorkflowRunId mismatch`);
assert(latest.publicationEvidenceArtifact === expectedLatestStable.publicationEvidenceArtifact, `${expectedLatestStable.version} evidence artifact mismatch`);
assert(latest.publicationEvidenceFile === expectedLatestStable.publicationEvidenceFile, `${expectedLatestStable.version} evidence file mismatch`);
NODE

require_pattern "$RELEASE_GATE" "release-evidence-index=docs/release_evidence_index.json" "release gate missing release evidence index marker"
require_pattern "$RELEASE_GATE" "release-evidence-index-static-check=scripts/release-evidence-index-check.sh" "release gate missing release evidence index static check marker"
require_pattern "$RELEASE_CHECKLIST" "docs/release_evidence_index.json" "release checklist missing release evidence index"
require_pattern "$CHECK_SCRIPT" "sh scripts/release-evidence-index-check.sh" "local full check missing release evidence index check"
require_pattern "$V1_ACCEPTANCE" "release-evidence-index-check.sh" "v1 acceptance missing release evidence index check"
require_pattern "$BUILD_WORKFLOW" "mitm_anixops/docs/release_evidence_index.json" "build workflow missing release evidence index file requirement"
require_pattern "$BUILD_WORKFLOW" "mitm_anixops/scripts/release-evidence-index-check.sh" "build workflow missing release evidence index check requirement"
require_pattern "$BUILD_WORKFLOW" "sh mitm_anixops/scripts/release-evidence-index-check.sh" "build workflow missing release evidence index check invocation"
require_pattern "$DRY_RUN_WORKFLOW" "sh scripts/release-evidence-index-check.sh" "release dry-run missing release evidence index check"
require_pattern "$RELEASE_WORKFLOW" "sh scripts/release-evidence-index-check.sh" "release workflow missing release evidence index check"

printf '%s\n' "release evidence index check passed"
