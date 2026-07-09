#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
REPO=$(CDPATH= cd -- "$ROOT/.." && pwd)
NEXT_RELEASE_VERSION=${1:-${RELEASE_VERSION:-}}

INDEX="$ROOT/docs/release_evidence_index.json"
RELEASE_GATE="$ROOT/docs/architecture/release-gate.md"
RELEASE_CHECKLIST="$ROOT/docs/release_checklist.md"
CHECK_SCRIPT="$ROOT/scripts/check.sh"
V1_ACCEPTANCE="$ROOT/scripts/v1-acceptance-check.sh"
INDEX_TEST="$ROOT/scripts/release-evidence-index-check-test.sh"
BUILD_WORKFLOW="$REPO/.github/workflows/build.yml"
DRY_RUN_WORKFLOW="$REPO/.github/workflows/release-dry-run.yml"
RELEASE_WORKFLOW="$REPO/.github/workflows/release.yml"

for file in \
	"$INDEX" \
	"$RELEASE_GATE" \
	"$RELEASE_CHECKLIST" \
	"$CHECK_SCRIPT" \
	"$V1_ACCEPTANCE" \
	"$INDEX_TEST" \
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

node - "$INDEX" "$NEXT_RELEASE_VERSION" <<'NODE'
const fs = require("fs");
const path = process.argv[2];
const nextReleaseVersion = process.argv[3] || "";
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
  version: "v1.3.5",
  targetCommit: "6b03dc08764665aed49128de7a191ad99096f0ab",
  ciRunId: "29042205659",
  releaseWorkflowRunId: "29042493837",
  freshnessStatus: "enforced-for-stable-patch-releases",
  nextStableRelease: "v1.3.6",
  releaseNotesFeatureAdditionsSection: "Feature additions:",
  releaseNotesBugFixesSection: "BUG fixes:",
  releaseNotesVerifiedSince: "v1.3.4",
  publicationEvidenceArtifact: "anixops-mitm-release-publication-evidence",
  publicationEvidenceFile: "release-publication-verify.env",
};

function parseStableVersion(version) {
  const match = /^v([0-9]+)\.([0-9]+)\.([0-9]+)$/.exec(version);
  if (!match) {
    return null;
  }
  return {
    major: Number(match[1]),
    minor: Number(match[2]),
    patch: Number(match[3]),
  };
}

function compareStableVersions(left, right) {
  const leftParts = parseStableVersion(left);
  const rightParts = parseStableVersion(right);
  assert(leftParts, `invalid stable version for comparison: ${left}`);
  assert(rightParts, `invalid stable version for comparison: ${right}`);

  for (const key of ["major", "minor", "patch"]) {
    if (leftParts[key] !== rightParts[key]) {
      return leftParts[key] - rightParts[key];
    }
  }
  return 0;
}

function previousPatchVersion(version) {
  const parsed = parseStableVersion(version);
  if (!parsed || parsed.patch === 0) {
    return "";
  }
  return `v${parsed.major}.${parsed.minor}.${parsed.patch - 1}`;
}

if (nextReleaseVersion) {
  assert(
    /^v[0-9]+\.[0-9]+\.[0-9]+(-(alpha|beta|rc)(\.[0-9]+)?)?$/.test(nextReleaseVersion),
    `invalid release evidence next release version: ${nextReleaseVersion}`,
  );
}

assert(index.schemaVersion === "release-evidence-index-v1", "invalid release evidence index schemaVersion");
assert(index.latestStable === expectedLatestStable.version, `latestStable must be ${expectedLatestStable.version}`);
assert(index.freshnessPolicy && typeof index.freshnessPolicy === "object", "freshnessPolicy missing");
assert(index.freshnessPolicy.status === expectedLatestStable.freshnessStatus, "freshnessPolicy status mismatch");
assert(index.freshnessPolicy.nextStableRelease === expectedLatestStable.nextStableRelease, "freshnessPolicy nextStableRelease mismatch");
assert(index.freshnessPolicy.requiredLatestStableBeforeNext === expectedLatestStable.version, "freshnessPolicy required latest stable mismatch");
assert(Array.isArray(index.entries), "entries must be an array");
assert(index.entries.length >= 3, "entries must include latest and retained historical release evidence");
assert(index.entries[0].version === index.latestStable, "first release evidence entry must match latestStable");

const expectedLatestForNext = previousPatchVersion(nextReleaseVersion);
if (expectedLatestForNext) {
  assert(
    index.latestStable === expectedLatestForNext,
    `latestStable must be ${expectedLatestForNext} for release ${nextReleaseVersion}`,
  );
}

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
  if (compareStableVersions(entry.version, "v1.3.4") >= 0) {
    assert(entry.releaseNotesChangeSummary && typeof entry.releaseNotesChangeSummary === "object", `${entry.version} release notes change summary missing`);
    assert(entry.releaseNotesChangeSummary.featureAdditionsSection === expectedLatestStable.releaseNotesFeatureAdditionsSection, `${entry.version} feature additions section mismatch`);
    assert(entry.releaseNotesChangeSummary.bugFixesSection === expectedLatestStable.releaseNotesBugFixesSection, `${entry.version} BUG fixes section mismatch`);
    assert(entry.releaseNotesChangeSummary.verifiedSince === expectedLatestStable.releaseNotesVerifiedSince, `${entry.version} release notes verifiedSince mismatch`);
  }
}

const latest = index.entries.find((entry) => entry.version === index.latestStable);
assert(latest, "missing latest stable release evidence entry");
assert(versions.has("v1.3.3"), "missing retained v1.3.3 release evidence entry");
assert(versions.has("v1.3.4"), "missing retained v1.3.4 release evidence entry");
assert(latest.targetCommit === expectedLatestStable.targetCommit, `${expectedLatestStable.version} targetCommit mismatch`);
assert(latest.ciRunId === expectedLatestStable.ciRunId, `${expectedLatestStable.version} ciRunId mismatch`);
assert(latest.releaseWorkflowRunId === expectedLatestStable.releaseWorkflowRunId, `${expectedLatestStable.version} releaseWorkflowRunId mismatch`);
assert(latest.releaseNotesChangeSummary && typeof latest.releaseNotesChangeSummary === "object", `${expectedLatestStable.version} release notes change summary missing`);
assert(latest.releaseNotesChangeSummary.featureAdditionsSection === expectedLatestStable.releaseNotesFeatureAdditionsSection, `${expectedLatestStable.version} feature additions section mismatch`);
assert(latest.releaseNotesChangeSummary.bugFixesSection === expectedLatestStable.releaseNotesBugFixesSection, `${expectedLatestStable.version} BUG fixes section mismatch`);
assert(latest.releaseNotesChangeSummary.verifiedSince === expectedLatestStable.releaseNotesVerifiedSince, `${expectedLatestStable.version} release notes verifiedSince mismatch`);
assert(latest.publicationEvidenceArtifact === expectedLatestStable.publicationEvidenceArtifact, `${expectedLatestStable.version} evidence artifact mismatch`);
assert(latest.publicationEvidenceFile === expectedLatestStable.publicationEvidenceFile, `${expectedLatestStable.version} evidence file mismatch`);
NODE

require_pattern "$RELEASE_GATE" "release-evidence-index=docs/release_evidence_index.json" "release gate missing release evidence index marker"
require_pattern "$RELEASE_GATE" "release-evidence-index-static-check=scripts/release-evidence-index-check.sh" "release gate missing release evidence index static check marker"
require_pattern "$RELEASE_GATE" "release-evidence-index-freshness-policy=stable-patch-release-requires-previous-patch-as-latestStable" "release gate missing release evidence index freshness policy marker"
require_pattern "$RELEASE_CHECKLIST" "docs/release_evidence_index.json" "release checklist missing release evidence index"
require_pattern "$RELEASE_CHECKLIST" "freshnessPolicy.requiredLatestStableBeforeNext" "release checklist missing release evidence index freshness policy"
require_pattern "$CHECK_SCRIPT" "sh scripts/release-evidence-index-check.sh" "local full check missing release evidence index check"
require_pattern "$CHECK_SCRIPT" "sh scripts/release-evidence-index-check-test.sh" "local full check missing release evidence index check test"
require_pattern "$V1_ACCEPTANCE" "release-evidence-index-check.sh" "v1 acceptance missing release evidence index check"
require_pattern "$V1_ACCEPTANCE" "release-evidence-index-check-test.sh" "v1 acceptance missing release evidence index check test"
require_pattern "$BUILD_WORKFLOW" "mitm_anixops/docs/release_evidence_index.json" "build workflow missing release evidence index file requirement"
require_pattern "$BUILD_WORKFLOW" "mitm_anixops/scripts/release-evidence-index-check.sh" "build workflow missing release evidence index check requirement"
require_pattern "$BUILD_WORKFLOW" "mitm_anixops/scripts/release-evidence-index-check-test.sh" "build workflow missing release evidence index check test requirement"
require_pattern "$BUILD_WORKFLOW" "sh mitm_anixops/scripts/release-evidence-index-check.sh" "build workflow missing release evidence index check invocation"
require_pattern "$BUILD_WORKFLOW" "sh mitm_anixops/scripts/release-evidence-index-check-test.sh" "build workflow missing release evidence index check test invocation"
require_pattern "$BUILD_WORKFLOW" "release_evidence_index_check_test_status=passed" "build workflow missing release evidence index check test status assertion"
require_pattern "$DRY_RUN_WORKFLOW" 'sh scripts/release-evidence-index-check.sh "${RELEASE_VERSION}"' "release dry-run missing versioned release evidence index check"
require_pattern "$RELEASE_WORKFLOW" 'sh scripts/release-evidence-index-check.sh "${RELEASE_VERSION}"' "release workflow missing versioned release evidence index check"

printf '%s\n' "release evidence index check passed"
