#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
REPO=$(CDPATH= cd -- "$ROOT/.." && pwd)

ROADMAP=${PRODUCTION_MITM_ROADMAP_FILE:-"$ROOT/ROADMAP.md"}
TODO_FILE=${PRODUCTION_MITM_TODO_FILE:-"$ROOT/TODO.md"}
CHANGELOG=${PRODUCTION_MITM_CHANGELOG_FILE:-"$ROOT/CHANGELOG.md"}
RELEASE_GATE=${PRODUCTION_MITM_RELEASE_GATE_FILE:-"$ROOT/docs/architecture/release-gate.md"}
RELEASE_DRY_RUN=${PRODUCTION_MITM_RELEASE_DRY_RUN_FILE:-"$ROOT/docs/architecture/release-dry-run.md"}
BUILD_WORKFLOW=${PRODUCTION_MITM_BUILD_WORKFLOW:-"$REPO/.github/workflows/build.yml"}
DRY_RUN_WORKFLOW=${PRODUCTION_MITM_DRY_RUN_WORKFLOW:-"$REPO/.github/workflows/release-dry-run.yml"}
RELEASE_WORKFLOW=${PRODUCTION_MITM_RELEASE_WORKFLOW:-"$REPO/.github/workflows/release.yml"}
CHECK_SCRIPT=${PRODUCTION_MITM_CHECK_SCRIPT:-"$ROOT/scripts/check.sh"}
V1_ACCEPTANCE=${PRODUCTION_MITM_V1_ACCEPTANCE_FILE:-"$ROOT/scripts/v1-acceptance-check.sh"}

fail() {
	printf '%s\n' "production MITM roadmap check failed: $1" >&2
	exit 1
}

require_file() {
	file=$1
	label=$2

	if [ ! -s "$file" ]; then
		fail "missing $label: $file"
	fi
}

require_pattern() {
	file=$1
	pattern=$2
	message=$3

	if ! grep -F -- "$pattern" "$file" >/dev/null; then
		fail "$message"
	fi
}

for pair in \
	"$ROADMAP:roadmap" \
	"$TODO_FILE:todo" \
	"$CHANGELOG:changelog" \
	"$RELEASE_GATE:release gate contract" \
	"$RELEASE_DRY_RUN:release dry-run contract" \
	"$BUILD_WORKFLOW:build workflow" \
	"$DRY_RUN_WORKFLOW:release dry-run workflow" \
	"$RELEASE_WORKFLOW:release workflow" \
	"$CHECK_SCRIPT:local full check" \
	"$V1_ACCEPTANCE:v1 acceptance check"
do
	file=${pair%%:*}
	label=${pair#*:}
	require_file "$file" "$label"
done

require_pattern "$ROADMAP" "## Production MITM Version Line" "roadmap missing production MITM version line"
require_pattern "$ROADMAP" "roadmap-production-mitm-gate=scripts/production-mitm-roadmap-check.sh" "roadmap missing production MITM gate marker"
require_pattern "$ROADMAP" "roadmap-production-mitm-contract-freeze-version=v2.0.0" "roadmap missing v2.0.0 contract-freeze marker"
require_pattern "$ROADMAP" "roadmap-production-mitm-beta-version=v2.8.0" "roadmap missing v2.8.0 beta marker"
require_pattern "$ROADMAP" "roadmap-production-mitm-rc-version=v2.9.0" "roadmap missing v2.9.0 RC marker"
require_pattern "$ROADMAP" "roadmap-production-mitm-ga-version=v3.0.0" "roadmap missing v3.0.0 GA marker"
require_pattern "$ROADMAP" "roadmap-production-mitm-pre-v3-claim=not-production-ready" "roadmap missing pre-v3 production boundary marker"
require_pattern "$ROADMAP" "v2.8.0 beta" "roadmap missing v2.8.0 beta text"
require_pattern "$ROADMAP" "v2.9.0 RC" "roadmap missing v2.9.0 RC text"
require_pattern "$ROADMAP" "v3.0.0 production-ready" "roadmap missing v3.0.0 production-ready text"
require_pattern "$ROADMAP" "freezes the production adapter contract" "roadmap missing v2.0.0 contract-freeze boundary"

require_pattern "$TODO_FILE" "Add a production MITM roadmap gate" "TODO missing production MITM roadmap gate entry"
require_pattern "$CHANGELOG" "production MITM roadmap gate" "changelog missing v1.2.7 production MITM roadmap entry"

require_pattern "$RELEASE_GATE" "ci-workflow-production-mitm-roadmap-gate=scripts/production-mitm-roadmap-check.sh" "release gate missing CI production MITM roadmap marker"
require_pattern "$RELEASE_GATE" "release-workflow-production-mitm-roadmap-gate=scripts/production-mitm-roadmap-check.sh" "release gate missing release production MITM roadmap marker"
require_pattern "$RELEASE_DRY_RUN" "release-dry-run-production-mitm-roadmap-gate=scripts/production-mitm-roadmap-check.sh" "release dry-run missing production MITM roadmap marker"

require_pattern "$BUILD_WORKFLOW" "mitm_anixops/scripts/production-mitm-roadmap-check.sh" "build workflow missing production MITM roadmap check"
require_pattern "$BUILD_WORKFLOW" "sh mitm_anixops/scripts/production-mitm-roadmap-check-test.sh" "build workflow missing production MITM roadmap self-test"
require_pattern "$BUILD_WORKFLOW" "sh mitm_anixops/scripts/production-mitm-roadmap-check.sh" "build workflow missing production MITM roadmap gate"
require_pattern "$DRY_RUN_WORKFLOW" "sh scripts/production-mitm-roadmap-check.sh" "release dry-run missing production MITM roadmap gate"
require_pattern "$RELEASE_WORKFLOW" "sh scripts/production-mitm-roadmap-check.sh" "release workflow missing production MITM roadmap gate"
require_pattern "$CHECK_SCRIPT" "sh scripts/production-mitm-roadmap-check-test.sh" "local full check missing production MITM roadmap self-test"
require_pattern "$CHECK_SCRIPT" "sh scripts/production-mitm-roadmap-check.sh" "local full check missing production MITM roadmap gate"
require_pattern "$V1_ACCEPTANCE" "production-mitm-roadmap-check.sh" "v1 acceptance missing production MITM roadmap gate"

scan_list=$(mktemp)
trap 'rm -f "$scan_list"' EXIT HUP INT TERM

if [ -n "${PRODUCTION_MITM_SCAN_PATHS:-}" ]; then
	printf '%s\n' "$PRODUCTION_MITM_SCAN_PATHS" | tr ':' '\n' > "$scan_list"
else
	for rel_path in README.md ROADMAP.md TODO.md CHANGELOG.md CONTRIBUTING.md docs; do
		printf '%s/%s\n' "$ROOT" "$rel_path"
	done > "$scan_list"
fi

early_version_claims='v1\.[0-9]+\.[0-9]+.*(ships|delivers|declares|is|as).*(production-ready|production ready|production-grade|production grade).*MITM|v2\.[0-7]\.[0-9]+.*(ships|delivers|declares|is|as).*(production-ready|production ready|production-grade|production grade).*MITM|production-ready MITM.*v2\.[0-7]\.[0-9]+|production ready MITM.*v2\.[0-7]\.[0-9]+|production-grade MITM.*v2\.[0-7]\.[0-9]+|production grade MITM.*v2\.[0-7]\.[0-9]+'

while IFS= read -r path; do
	[ -n "$path" ] || continue
	if [ ! -e "$path" ]; then
		fail "scan path does not exist: $path"
	fi
	if LC_ALL=C grep -RInEi -- "$early_version_claims" "$path"; then
		fail "early production MITM claim found"
	fi
done < "$scan_list"

printf '%s\n' "production MITM roadmap check passed"
