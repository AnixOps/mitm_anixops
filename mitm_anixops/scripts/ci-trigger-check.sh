#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
REPO=$(CDPATH= cd -- "$ROOT/.." && pwd)

BUILD_WORKFLOW="$REPO/.github/workflows/build.yml"
DRY_RUN_WORKFLOW="$REPO/.github/workflows/release-dry-run.yml"
RELEASE_WORKFLOW="$REPO/.github/workflows/release.yml"
RELEASE_GATE_DOC="$ROOT/docs/architecture/release-gate.md"
RELEASE_DRY_RUN_DOC="$ROOT/docs/architecture/release-dry-run.md"
CHECK_SCRIPT="$ROOT/scripts/check.sh"

for file in \
	"$BUILD_WORKFLOW" \
	"$DRY_RUN_WORKFLOW" \
	"$RELEASE_WORKFLOW" \
	"$RELEASE_GATE_DOC" \
	"$RELEASE_DRY_RUN_DOC" \
	"$CHECK_SCRIPT"
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

require_regex() {
	file=$1
	pattern=$2
	message=$3

	if ! grep -Eq -- "$pattern" "$file"; then
		printf '%s: %s\n' "$message" "$pattern" >&2
		exit 1
	fi
}

require_absent() {
	file=$1
	pattern=$2
	message=$3

	if grep -F -- "$pattern" "$file" >/dev/null; then
		printf '%s: %s\n' "$message" "$pattern" >&2
		exit 1
	fi
}

require_pattern "$BUILD_WORKFLOW" "name: build" "build workflow missing name"
require_regex "$BUILD_WORKFLOW" '^  pull_request:$' "build workflow must run on pull requests"
require_regex "$BUILD_WORKFLOW" '^  push:$' "build workflow must run on push, including main and tag pushes"
require_regex "$BUILD_WORKFLOW" '^  workflow_dispatch:$' "build workflow must allow manual validation"
for job in \
	governance \
	lint \
	format-check \
	compatibility-matrix \
	linux-test \
	macos-smoke \
	windows-binary
do
	require_regex "$BUILD_WORKFLOW" "^  ${job}:" "build workflow missing required job"
done
require_pattern "$BUILD_WORKFLOW" "sh mitm_anixops/scripts/script-trigger-evidence-check-test.sh" "build workflow missing script trigger evidence gate self-test"
require_pattern "$BUILD_WORKFLOW" "sh mitm_anixops/scripts/script-trigger-evidence-check.sh" "build workflow missing script trigger evidence gate"
require_pattern "$CHECK_SCRIPT" "sh scripts/script-trigger-evidence-check-test.sh" "local full check missing script trigger evidence gate self-test"
require_pattern "$CHECK_SCRIPT" "sh scripts/script-trigger-evidence-check.sh" "local full check missing script trigger evidence gate"
require_pattern "$BUILD_WORKFLOW" "sh mitm_anixops/scripts/script-runtime-security-gate-test.sh" "build workflow missing script runtime security gate self-test"
require_pattern "$BUILD_WORKFLOW" "sh mitm_anixops/scripts/script-runtime-security-gate.sh" "build workflow missing script runtime security gate"
require_pattern "$DRY_RUN_WORKFLOW" "sh scripts/script-runtime-security-gate.sh" "release dry-run missing script runtime security gate"
require_pattern "$RELEASE_WORKFLOW" "sh scripts/script-runtime-security-gate.sh" "release workflow missing script runtime security gate"
require_pattern "$CHECK_SCRIPT" "sh scripts/script-runtime-security-gate-test.sh" "local full check missing script runtime security gate self-test"
require_pattern "$CHECK_SCRIPT" "sh scripts/script-runtime-security-gate.sh" "local full check missing script runtime security gate"
require_pattern "$BUILD_WORKFLOW" "sh mitm_anixops/scripts/integration-adapter-readiness-check-test.sh" "build workflow missing integration adapter readiness self-test"
require_pattern "$BUILD_WORKFLOW" "sh mitm_anixops/scripts/integration-adapter-readiness-check.sh" "build workflow missing integration adapter readiness gate"
require_pattern "$DRY_RUN_WORKFLOW" "sh scripts/integration-adapter-readiness-check.sh" "release dry-run missing integration adapter readiness gate"
require_pattern "$RELEASE_WORKFLOW" "sh scripts/integration-adapter-readiness-check.sh" "release workflow missing integration adapter readiness gate"
require_pattern "$CHECK_SCRIPT" "sh scripts/integration-adapter-readiness-check-test.sh" "local full check missing integration adapter readiness self-test"
require_pattern "$CHECK_SCRIPT" "sh scripts/integration-adapter-readiness-check.sh" "local full check missing integration adapter readiness gate"

require_pattern "$DRY_RUN_WORKFLOW" "name: release-dry-run" "release dry-run workflow missing name"
require_regex "$DRY_RUN_WORKFLOW" '^  workflow_dispatch:$' "release dry-run must allow manual validation"
require_regex "$DRY_RUN_WORKFLOW" '^  pull_request:$' "release dry-run must run on pull requests"
require_regex "$DRY_RUN_WORKFLOW" '^  push:$' "release dry-run must run on main pushes"
require_pattern "$DRY_RUN_WORKFLOW" "branches:" "release dry-run must scope branch triggers"
require_pattern "$DRY_RUN_WORKFLOW" "- main" "release dry-run must target main"
require_pattern "$DRY_RUN_WORKFLOW" "release dry-run pull requests must target main" "release dry-run missing PR branch guard"
require_pattern "$DRY_RUN_WORKFLOW" "release dry-run must run from main" "release dry-run missing main branch guard"
require_pattern "$DRY_RUN_WORKFLOW" "publication=blocked" "release dry-run must report blocked publication"
require_pattern "$DRY_RUN_WORKFLOW" "ci_gate=equivalent-full-check-in-workflow" "release dry-run missing same-workflow CI gate marker"
require_pattern "$DRY_RUN_WORKFLOW" "Run same-workflow CI gate" "release dry-run missing same-workflow CI gate step"
require_absent "$DRY_RUN_WORKFLOW" "tags:" "release dry-run must not run from release tags"
require_absent "$DRY_RUN_WORKFLOW" "contents: write" "release dry-run must not request content write permission"
require_absent "$DRY_RUN_WORKFLOW" "gh release create" "release dry-run must not publish GitHub releases"

require_pattern "$RELEASE_WORKFLOW" "name: release" "release workflow missing name"
require_regex "$RELEASE_WORKFLOW" '^  push:$' "release workflow must run on push"
require_pattern "$RELEASE_WORKFLOW" "tags:" "release workflow must scope push trigger to tags"
require_pattern "$RELEASE_WORKFLOW" "- \"v*\"" "release workflow must run on v* tags"
require_regex "$RELEASE_WORKFLOW" '^  workflow_dispatch:$' "release workflow must allow manual validation"
require_pattern "$RELEASE_WORKFLOW" "manual release workflow validation must run from main" "release workflow missing manual-main guard"
require_pattern "$RELEASE_WORKFLOW" "release push must be a v* tag" "release workflow missing tag push guard"
require_pattern "$RELEASE_WORKFLOW" "source_mode=\"tag\"" "release workflow missing tag source mode"
require_pattern "$RELEASE_WORKFLOW" "publication=github-release-assets" "release workflow missing tag publication mode"
require_pattern "$RELEASE_WORKFLOW" "same-commit-ci:" "release workflow missing same-commit CI job"
require_pattern "$RELEASE_WORKFLOW" "actions/workflows/build.yml/runs" "release workflow missing build workflow lookup"
require_pattern "$RELEASE_WORKFLOW" "head_branch == \"main\"" "release workflow missing main-branch CI lookup guard"
require_pattern "$RELEASE_WORKFLOW" "environment: github-release-publication" "release workflow missing publication environment"
require_pattern "$RELEASE_WORKFLOW" "contents: write" "release workflow publish job missing content write permission"
require_pattern "$RELEASE_WORKFLOW" "gh release create" "release workflow missing GitHub Release publication step"

require_pattern "$RELEASE_GATE_DOC" "ci-workflow-trigger-static-check=scripts/ci-trigger-check.sh" "release gate missing CI trigger static check marker"
require_pattern "$RELEASE_GATE_DOC" "ci-workflow-script-runtime-security-gate=scripts/script-runtime-security-gate.sh" "release gate missing CI script runtime security marker"
require_pattern "$RELEASE_GATE_DOC" "ci-workflow-integration-adapter-readiness-gate=scripts/integration-adapter-readiness-check.sh" "release gate missing CI integration adapter readiness marker"
require_pattern "$RELEASE_GATE_DOC" "release-workflow-trigger-static-check=scripts/ci-trigger-check.sh" "release gate missing release trigger static check marker"
require_pattern "$RELEASE_GATE_DOC" "release-workflow-script-runtime-security-gate=scripts/script-runtime-security-gate.sh" "release gate missing release script runtime security marker"
require_pattern "$RELEASE_GATE_DOC" "release-workflow-integration-adapter-readiness-gate=scripts/integration-adapter-readiness-check.sh" "release gate missing release integration adapter readiness marker"
require_pattern "$RELEASE_DRY_RUN_DOC" "release-dry-run-trigger-static-check=scripts/ci-trigger-check.sh" "release dry-run contract missing trigger static check marker"
require_pattern "$RELEASE_DRY_RUN_DOC" "release-dry-run-script-runtime-security-gate=scripts/script-runtime-security-gate.sh" "release dry-run contract missing script runtime security marker"
require_pattern "$RELEASE_DRY_RUN_DOC" "release-dry-run-integration-adapter-readiness-gate=scripts/integration-adapter-readiness-check.sh" "release dry-run contract missing integration adapter readiness marker"

printf '%s\n' "ci trigger check passed"
