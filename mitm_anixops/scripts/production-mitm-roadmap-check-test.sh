#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CHECK="$ROOT/scripts/production-mitm-roadmap-check.sh"
TMPDIR_ROOT=${TMPDIR:-/tmp}
TMPDIR=$(mktemp -d "$TMPDIR_ROOT/mitm-production-roadmap.XXXXXX")
trap 'rm -rf "$TMPDIR"' EXIT HUP INT TERM

mkdir -p "$TMPDIR/docs/architecture" "$TMPDIR/.github/workflows" "$TMPDIR/scripts"

write_common_fixture() {
	roadmap=$1
	todo_file=$2
	changelog=$3
	release_gate=$4
	release_dry_run=$5
	build_workflow=$6
	dry_run_workflow=$7
	release_workflow=$8
	check_script=$9
	v1_acceptance=${10}

	{
		printf '%s\n' '# Roadmap'
		printf '%s\n' '## Production MITM Version Line'
		printf '%s\n' 'roadmap-production-mitm-gate=scripts/production-mitm-roadmap-check.sh'
		printf '%s\n' 'roadmap-production-mitm-contract-freeze-version=v2.0.0'
		printf '%s\n' 'roadmap-production-mitm-beta-version=v2.8.0'
		printf '%s\n' 'roadmap-production-mitm-rc-version=v2.9.0'
		printf '%s\n' 'roadmap-production-mitm-ga-version=v3.0.0'
		printf '%s\n' 'roadmap-production-mitm-pre-v3-claim=not-production-ready'
		printf '%s\n' 'v2.0.0 freezes the production adapter contract.'
		printf '%s\n' 'v2.8.0 beta starts production-grade MITM validation.'
		printf '%s\n' 'v2.9.0 RC is the release candidate.'
		printf '%s\n' 'v3.0.0 production-ready is the first GA claim.'
	} > "$roadmap"
	printf '%s\n' '- [x] Add a production MITM roadmap gate.' > "$todo_file"
	printf '%s\n' '- Added a v1.2.7 production MITM roadmap gate.' > "$changelog"
	{
		printf '%s\n' 'ci-workflow-production-mitm-roadmap-gate=scripts/production-mitm-roadmap-check.sh'
		printf '%s\n' 'release-workflow-production-mitm-roadmap-gate=scripts/production-mitm-roadmap-check.sh'
	} > "$release_gate"
	printf '%s\n' 'release-dry-run-production-mitm-roadmap-gate=scripts/production-mitm-roadmap-check.sh' > "$release_dry_run"
	{
		printf '%s\n' 'mitm_anixops/scripts/production-mitm-roadmap-check.sh'
		printf '%s\n' 'sh mitm_anixops/scripts/production-mitm-roadmap-check-test.sh'
		printf '%s\n' 'sh mitm_anixops/scripts/production-mitm-roadmap-check.sh'
	} > "$build_workflow"
	printf '%s\n' 'sh scripts/production-mitm-roadmap-check.sh' > "$dry_run_workflow"
	printf '%s\n' 'sh scripts/production-mitm-roadmap-check.sh' > "$release_workflow"
	{
		printf '%s\n' 'sh scripts/production-mitm-roadmap-check-test.sh'
		printf '%s\n' 'sh scripts/production-mitm-roadmap-check.sh'
	} > "$check_script"
	printf '%s\n' 'sh "$ROOT/scripts/production-mitm-roadmap-check.sh" >/dev/null' > "$v1_acceptance"
}

ROADMAP="$TMPDIR/ROADMAP.md"
TODO_FILE="$TMPDIR/TODO.md"
CHANGELOG="$TMPDIR/CHANGELOG.md"
RELEASE_GATE="$TMPDIR/docs/architecture/release-gate.md"
RELEASE_DRY_RUN="$TMPDIR/docs/architecture/release-dry-run.md"
BUILD_WORKFLOW="$TMPDIR/.github/workflows/build.yml"
DRY_RUN_WORKFLOW="$TMPDIR/.github/workflows/release-dry-run.yml"
RELEASE_WORKFLOW="$TMPDIR/.github/workflows/release.yml"
CHECK_SCRIPT="$TMPDIR/scripts/check.sh"
V1_ACCEPTANCE="$TMPDIR/scripts/v1-acceptance-check.sh"

write_common_fixture \
	"$ROADMAP" \
	"$TODO_FILE" \
	"$CHANGELOG" \
	"$RELEASE_GATE" \
	"$RELEASE_DRY_RUN" \
	"$BUILD_WORKFLOW" \
	"$DRY_RUN_WORKFLOW" \
	"$RELEASE_WORKFLOW" \
	"$CHECK_SCRIPT" \
	"$V1_ACCEPTANCE"

PRODUCTION_MITM_ROADMAP_FILE="$ROADMAP" \
PRODUCTION_MITM_TODO_FILE="$TODO_FILE" \
PRODUCTION_MITM_CHANGELOG_FILE="$CHANGELOG" \
PRODUCTION_MITM_RELEASE_GATE_FILE="$RELEASE_GATE" \
PRODUCTION_MITM_RELEASE_DRY_RUN_FILE="$RELEASE_DRY_RUN" \
PRODUCTION_MITM_BUILD_WORKFLOW="$BUILD_WORKFLOW" \
PRODUCTION_MITM_DRY_RUN_WORKFLOW="$DRY_RUN_WORKFLOW" \
PRODUCTION_MITM_RELEASE_WORKFLOW="$RELEASE_WORKFLOW" \
PRODUCTION_MITM_CHECK_SCRIPT="$CHECK_SCRIPT" \
PRODUCTION_MITM_V1_ACCEPTANCE_FILE="$V1_ACCEPTANCE" \
PRODUCTION_MITM_SCAN_PATHS="$ROADMAP" \
sh "$CHECK" >/dev/null

printf '%s\n' 'v2.7.0 ships production-ready MITM.' >> "$ROADMAP"

if PRODUCTION_MITM_ROADMAP_FILE="$ROADMAP" \
	PRODUCTION_MITM_TODO_FILE="$TODO_FILE" \
	PRODUCTION_MITM_CHANGELOG_FILE="$CHANGELOG" \
	PRODUCTION_MITM_RELEASE_GATE_FILE="$RELEASE_GATE" \
	PRODUCTION_MITM_RELEASE_DRY_RUN_FILE="$RELEASE_DRY_RUN" \
	PRODUCTION_MITM_BUILD_WORKFLOW="$BUILD_WORKFLOW" \
	PRODUCTION_MITM_DRY_RUN_WORKFLOW="$DRY_RUN_WORKFLOW" \
	PRODUCTION_MITM_RELEASE_WORKFLOW="$RELEASE_WORKFLOW" \
	PRODUCTION_MITM_CHECK_SCRIPT="$CHECK_SCRIPT" \
	PRODUCTION_MITM_V1_ACCEPTANCE_FILE="$V1_ACCEPTANCE" \
	PRODUCTION_MITM_SCAN_PATHS="$ROADMAP" \
	sh "$CHECK" >/dev/null 2>&1
then
	printf '%s\n' "production MITM roadmap check test failed: early production claim was not rejected" >&2
	exit 1
fi

printf '%s\n' "production MITM roadmap check test passed"
