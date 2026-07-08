#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

CHECKLIST="$ROOT/docs/release_checklist.md"
MANUAL="$ROOT/docs/manual-intervention.md"
RELEASE_GATE="$ROOT/docs/architecture/release-gate.md"

for file in \
	"$CHECKLIST" \
	"$MANUAL" \
	"$RELEASE_GATE"
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

require_absent() {
	file=$1
	pattern=$2
	message=$3

	if grep -F -- "$pattern" "$file" >/dev/null; then
		printf '%s: %s\n' "$message" "$pattern" >&2
		exit 1
	fi
}

require_pattern "$CHECKLIST" "GitHub Actions \`build\` workflow" "release checklist must use build workflow evidence"
require_pattern "$CHECKLIST" "GitHub Actions \`release-dry-run\` workflow" "release checklist must use release dry-run evidence"
require_pattern "$CHECKLIST" "GitHub Actions \`release\` workflow" "release checklist must use tag release workflow evidence"
require_pattern "$CHECKLIST" "scripts/release-readiness-check.sh v1.0.0" "release checklist missing v1 readiness command"
require_pattern "$CHECKLIST" "scripts/manual-intervention-check.sh" "release checklist missing manual intervention check command"
require_pattern "$CHECKLIST" "scripts/manual-intervention-transition-check.sh" "release checklist missing manual intervention transition check command"
require_pattern "$CHECKLIST" "scripts/release-checklist-check.sh" "release checklist missing release checklist check command"
require_pattern "$CHECKLIST" "scripts/release-metadata-check.sh" "release checklist missing release metadata check command"
require_pattern "$CHECKLIST" "release_readiness_status=passed" "release checklist missing readiness pass evidence"
require_pattern "$CHECKLIST" "branch-protection" "release checklist missing branch protection blocker"
require_pattern "$CHECKLIST" "protected-tags" "release checklist missing protected tags blocker"
require_pattern "$CHECKLIST" "release-environment-approval" "release checklist missing release environment blocker"
require_pattern "$CHECKLIST" "anixops-mitm-release-package" "release checklist missing release package artifact"
require_pattern "$CHECKLIST" "anixops-mitm-release-dry-run" "release checklist missing dry-run package artifact"
require_pattern "$CHECKLIST" "manifest.json" "release checklist missing manifest evidence"
require_pattern "$CHECKLIST" ".sha256" "release checklist missing checksum evidence"
require_pattern "$CHECKLIST" "release-notes.md" "release checklist missing release notes evidence"
require_pattern "$CHECKLIST" "docs/compatibility/matrix.md" "release checklist missing v1 compatibility matrix"
require_pattern "$CHECKLIST" "docs/manual-intervention.md" "release checklist missing manual intervention register"
require_pattern "$CHECKLIST" "Do not use local build or test output as release acceptance evidence." "release checklist must reject local acceptance evidence"

require_absent "$CHECKLIST" "   sh scripts/check.sh" "release checklist must not require local scripts/check.sh as acceptance"

for marker in branch-protection protected-tags release-environment-approval; do
	require_pattern "$MANUAL" "${marker}-status=" "manual register missing v1 governance marker"
	require_pattern "$CHECKLIST" "$marker" "release checklist missing governance marker"
done

require_pattern "$RELEASE_GATE" "release-workflow-v1-readiness-gate=required-before-v1-manual-markers-and-no-planned-matrix-rows" "release gate missing v1 readiness marker"
require_pattern "$RELEASE_GATE" "release-checklist-static-check=scripts/release-checklist-check.sh" "release gate missing release checklist static check marker"

printf '%s\n' "release checklist check passed"
