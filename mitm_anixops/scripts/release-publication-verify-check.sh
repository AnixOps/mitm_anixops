#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
REPO=$(CDPATH= cd -- "$ROOT/.." && pwd)

VERIFY_SCRIPT="$ROOT/scripts/release-publication-verify.sh"
TEST_SCRIPT="$ROOT/scripts/release-publication-verify-test.sh"
CHECK_SCRIPT="$ROOT/scripts/check.sh"
V1_ACCEPTANCE="$ROOT/scripts/v1-acceptance-check.sh"
RELEASE_CHECKLIST="$ROOT/docs/release_checklist.md"
RELEASE_GATE="$ROOT/docs/architecture/release-gate.md"
BUILD_WORKFLOW="$REPO/.github/workflows/build.yml"
DRY_RUN_WORKFLOW="$REPO/.github/workflows/release-dry-run.yml"
RELEASE_WORKFLOW="$REPO/.github/workflows/release.yml"

for file in \
	"$VERIFY_SCRIPT" \
	"$TEST_SCRIPT" \
	"$CHECK_SCRIPT" \
	"$V1_ACCEPTANCE" \
	"$RELEASE_CHECKLIST" \
	"$RELEASE_GATE" \
	"$BUILD_WORKFLOW" \
	"$DRY_RUN_WORKFLOW" \
	"$RELEASE_WORKFLOW"
do
	test -s "$file"
done

require_pattern() {
	file=$1
	pattern=$2
	message=$3

	if ! grep -F -- "$pattern" "$file" >/dev/null; then
		printf '%s: %s\n' "$message" "$pattern" >&2
		exit 1
	fi
}

require_pattern "$VERIFY_SCRIPT" "usage: sh scripts/release-publication-verify.sh VERSION EXPECTED_COMMIT EXPECTED_RELEASE_RUN_ID EXPECTED_CI_RUN_ID" "publication verifier missing usage"
require_pattern "$VERIFY_SCRIPT" "gh release view" "publication verifier missing release view"
require_pattern "$VERIFY_SCRIPT" "gh release download" "publication verifier missing release asset download"
require_pattern "$VERIFY_SCRIPT" "asset_count" "publication verifier missing asset count validation"
require_pattern "$VERIFY_SCRIPT" "downloaded_asset_count" "publication verifier missing downloaded asset count validation"
require_pattern "$VERIFY_SCRIPT" "verify_sha_sidecar" "publication verifier missing checksum sidecar validation"
require_pattern "$VERIFY_SCRIPT" "release_workflow_run_id" "publication verifier missing release run evidence"
require_pattern "$VERIFY_SCRIPT" "ci_run_id" "publication verifier missing CI run evidence"
require_pattern "$VERIFY_SCRIPT" "artifact_count == 2" "publication verifier missing artifact count manifest validation"
require_pattern "$VERIFY_SCRIPT" "artifact_platforms" "publication verifier missing artifact platform validation"
require_pattern "$VERIFY_SCRIPT" "linux-x64" "publication verifier missing Linux platform"
require_pattern "$VERIFY_SCRIPT" "windows-x64" "publication verifier missing Windows platform"
require_pattern "$VERIFY_SCRIPT" "release_publication_verify_status=passed" "publication verifier missing passed status output"

require_pattern "$TEST_SCRIPT" "RELEASE_PUBLICATION_VERIFY_FIXTURE_ASSETS" "publication verifier test missing fixture asset source"
require_pattern "$TEST_SCRIPT" "release_publication_verify_test_status=passed" "publication verifier test missing passed status output"
require_pattern "$TEST_SCRIPT" "sha256 mismatch for linux artifact" "publication verifier test missing checksum negative case"

require_pattern "$RELEASE_GATE" "release-publication-verify-script=scripts/release-publication-verify.sh" "release gate missing publication verifier script marker"
require_pattern "$RELEASE_GATE" "release-publication-verify-static-check=scripts/release-publication-verify-check.sh" "release gate missing publication verifier static check marker"
require_pattern "$RELEASE_GATE" "release-publication-verify-fixture-test=scripts/release-publication-verify-test.sh" "release gate missing publication verifier fixture test marker"
require_pattern "$RELEASE_GATE" "release-publication-post-publish-evidence=assets-manifest-notes-checksums-ci-run-release-run-artifact-platforms" "release gate missing publication verifier evidence marker"

require_pattern "$RELEASE_CHECKLIST" "scripts/release-publication-verify.sh v1.0.0 <commit> <release-run-id> <ci-run-id>" "release checklist missing publication verifier command"
require_pattern "$CHECK_SCRIPT" "sh scripts/release-publication-verify-check.sh" "local full check missing publication verifier static check"
require_pattern "$CHECK_SCRIPT" "sh scripts/release-publication-verify-test.sh" "local full check missing publication verifier fixture test"
require_pattern "$V1_ACCEPTANCE" "release-publication-verify-check.sh" "v1 acceptance missing publication verifier static check"
require_pattern "$V1_ACCEPTANCE" "release-publication-verify-test.sh" "v1 acceptance missing publication verifier fixture test"
require_pattern "$BUILD_WORKFLOW" "mitm_anixops/scripts/release-publication-verify.sh" "build workflow missing publication verifier script requirement"
require_pattern "$BUILD_WORKFLOW" "mitm_anixops/scripts/release-publication-verify-test.sh" "build workflow missing publication verifier test requirement"
require_pattern "$BUILD_WORKFLOW" "sh mitm_anixops/scripts/release-publication-verify-check.sh" "build workflow missing publication verifier static check"
require_pattern "$BUILD_WORKFLOW" "sh mitm_anixops/scripts/release-publication-verify-test.sh" "build workflow missing publication verifier fixture test"
require_pattern "$DRY_RUN_WORKFLOW" "sh scripts/release-publication-verify-check.sh" "release dry-run missing publication verifier static check"
require_pattern "$DRY_RUN_WORKFLOW" "sh scripts/release-publication-verify-test.sh" "release dry-run missing publication verifier fixture test"
require_pattern "$RELEASE_WORKFLOW" "sh scripts/release-publication-verify-check.sh" "release workflow missing publication verifier static check"
require_pattern "$RELEASE_WORKFLOW" "sh scripts/release-publication-verify-test.sh" "release workflow missing publication verifier fixture test"

printf '%s\n' "release publication verify check passed"
