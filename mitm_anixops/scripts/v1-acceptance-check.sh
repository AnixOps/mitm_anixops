#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
REPO=$(CDPATH= cd -- "$ROOT/.." && pwd)

README="$ROOT/README.md"
ROADMAP="$ROOT/ROADMAP.md"
TODO="$ROOT/TODO.md"
CHANGELOG="$ROOT/CHANGELOG.md"
CONTRIBUTING="$ROOT/CONTRIBUTING.md"
MATRIX="$ROOT/docs/compatibility/matrix.md"
MANUAL="$ROOT/docs/manual-intervention.md"
RELEASE_GATE="$ROOT/docs/architecture/release-gate.md"
RELEASE_DRY_RUN="$ROOT/docs/architecture/release-dry-run.md"
RELEASE_CHECKLIST="$ROOT/docs/release_checklist.md"
REPOSITORY_GOVERNANCE="$ROOT/docs/architecture/repository-governance.md"
BUILD_WORKFLOW="$REPO/.github/workflows/build.yml"
DRY_RUN_WORKFLOW="$REPO/.github/workflows/release-dry-run.yml"
RELEASE_WORKFLOW="$REPO/.github/workflows/release.yml"

for file in \
	"$README" \
	"$ROADMAP" \
	"$TODO" \
	"$CHANGELOG" \
	"$CONTRIBUTING" \
	"$MATRIX" \
	"$MANUAL" \
	"$RELEASE_GATE" \
	"$RELEASE_DRY_RUN" \
	"$RELEASE_CHECKLIST" \
	"$REPOSITORY_GOVERNANCE" \
	"$BUILD_WORKFLOW" \
	"$DRY_RUN_WORKFLOW" \
	"$RELEASE_WORKFLOW"
do
	test -s "$file"
done

grep -q "Standalone C ABI library" "$README"
grep -q "## Scope" "$README"
grep -q "## Build" "$README"
grep -q "Not implemented yet:" "$README"
grep -q "GitHub Actions" "$README"
grep -q "docs/compatibility/matrix.md" "$README"
! grep -q "For the supported configuration grammar and public-API evidence, see \`docs/compatibility_matrix.md\`" "$README"

for phase in \
	"P0 Repository Baseline" \
	"P1 Plugin Parser" \
	"P2 Rule Matching Engine" \
	"P3 Script And Runtime Compatibility" \
	"P4 MITM Policy And Certificate Lifecycle" \
	"P5 Integration Adapter" \
	"P6 Release Hardening"
do
	grep -q "$phase" "$ROADMAP"
done
grep -q "## v1.0.0 Minimum Acceptance Criteria" "$ROADMAP"
grep -q "roadmap-v1-release-status=not-ready" "$ROADMAP"

grep -q "supported" "$MATRIX"
grep -q "partial" "$MATRIX"
grep -q "unsupported" "$MATRIX"
sh "$ROOT/scripts/compatibility-matrix-check.sh" >/dev/null
sh "$ROOT/scripts/compatibility-evidence-check.sh" >/dev/null
sh "$ROOT/scripts/compatibility-status-summary-check.sh" >/dev/null
sh "$ROOT/scripts/ci-trigger-check.sh" >/dev/null
sh "$ROOT/scripts/release-checklist-check.sh" >/dev/null
sh "$ROOT/scripts/release-metadata-check.sh" >/dev/null
compatibility_status="$(sh "$ROOT/scripts/compatibility-status-summary.sh")"
printf '%s\n' "$compatibility_status" | grep -q "compatibility_total_count="
printf '%s\n' "$compatibility_status" | grep -q "compatibility_planned_count=0"

sh "$ROOT/scripts/manual-intervention-check.sh" >/dev/null
sh "$ROOT/scripts/manual-intervention-transition-check.sh" >/dev/null
repository_governance_status="$(sh "$ROOT/scripts/repository-governance-check.sh")"
printf '%s\n' "$repository_governance_status" | grep -Eq "repository_governance_status=(blocked|passed)"
for marker in branch-protection protected-tags release-environment-approval; do
	status="$(awk -F= -v key="${marker}-status" '$1 == key {print $2}' "$MANUAL")"
	case "$status" in
		pending | confirmed)
			;;
		*)
			printf 'unsupported v1 governance marker status: %s=%s\n' "$marker" "$status" >&2
			exit 1
			;;
	esac
	grep -q "${marker}-confirmation-evidence=" "$MANUAL"
done

grep -q "pull_request:" "$BUILD_WORKFLOW"
grep -q "push:" "$BUILD_WORKFLOW"
grep -q "workflow_dispatch:" "$BUILD_WORKFLOW"
grep -q "governance:" "$BUILD_WORKFLOW"
grep -q "lint:" "$BUILD_WORKFLOW"
grep -q "format-check:" "$BUILD_WORKFLOW"
grep -q "compatibility-matrix:" "$BUILD_WORKFLOW"
grep -q "linux-test:" "$BUILD_WORKFLOW"
grep -q "macos-smoke:" "$BUILD_WORKFLOW"
grep -q "windows-binary:" "$BUILD_WORKFLOW"

grep -q "release-dry-run-linux-artifact=linux-x64-tarball-with-checksum" "$RELEASE_DRY_RUN"
grep -q "release-dry-run-windows-artifact=windows-x64-zip-with-checksum" "$RELEASE_DRY_RUN"
grep -q "GitHub Actions \`build\` workflow" "$RELEASE_CHECKLIST"
grep -q "GitHub Actions \`release-dry-run\` workflow" "$RELEASE_CHECKLIST"
grep -q "GitHub Actions \`release\` workflow" "$RELEASE_CHECKLIST"
grep -q "docs/compatibility/matrix.md" "$RELEASE_CHECKLIST"
! grep -q "^   sh scripts/check.sh$" "$RELEASE_CHECKLIST"
grep -q "package-windows:" "$DRY_RUN_WORKFLOW"
grep -q "Validate dry-run metadata" "$DRY_RUN_WORKFLOW"
grep -q "\"platform\": \"windows-x64\"" "$DRY_RUN_WORKFLOW"
grep -q "gh release create" "$DRY_RUN_WORKFLOW" && {
	printf '%s\n' "release dry-run must not publish GitHub releases" >&2
	exit 1
}

grep -q "tags:" "$RELEASE_WORKFLOW"
grep -q "v\\*" "$RELEASE_WORKFLOW"
grep -q "same-commit-main-build-success" "$RELEASE_WORKFLOW"
grep -q "package-windows:" "$RELEASE_WORKFLOW"
grep -q "artifact_sha256" "$RELEASE_WORKFLOW"
grep -q "windows_artifact_sha256" "$RELEASE_WORKFLOW"
grep -q "manifest_sha256" "$RELEASE_WORKFLOW"
grep -q "release-notes.md" "$RELEASE_WORKFLOW"
grep -q "gh release create" "$RELEASE_WORKFLOW"
grep -q "stable release readiness did not pass" "$RELEASE_WORKFLOW"
grep -q "release-workflow-v1-readiness-gate=required-before-v1-manual-markers-and-no-planned-matrix-rows" "$RELEASE_GATE"
grep -q "release-workflow-windows-artifact=windows-x64-zip-with-checksum" "$RELEASE_GATE"
grep -q "release-workflow-compatibility-summary=status-counts-in-manifest-notes-summary" "$RELEASE_GATE"
grep -q "ci-workflow-trigger-static-check=scripts/ci-trigger-check.sh" "$RELEASE_GATE"
grep -q "ci-workflow-manual-intervention-transition-status=scripts/manual-intervention-transition-check.sh" "$RELEASE_GATE"
grep -q "release-workflow-trigger-static-check=scripts/ci-trigger-check.sh" "$RELEASE_GATE"
grep -q "release-checklist-static-check=scripts/release-checklist-check.sh" "$RELEASE_GATE"

sh "$ROOT/scripts/security-claim-check.sh" >/dev/null

alpha_readiness="$(sh "$ROOT/scripts/release-readiness-check.sh" v0.45.10-alpha.1)"
printf '%s\n' "$alpha_readiness" | grep -q "release_readiness_status=not-required-prerelease"

readiness_output="${TMPDIR:-/tmp}/mitm-anixops-v1-readiness-$$.env"
trap 'rm -f "$readiness_output"' EXIT HUP INT TERM
if sh "$ROOT/scripts/release-readiness-check.sh" v1.0.0 > "$readiness_output"; then
	v1_status=passed
else
	grep -q "release_readiness_status=blocked" "$readiness_output"
	grep -Eq "release_readiness_blocking_reason=(manual-intervention-required-before-v1|manual-intervention-required-before-v1-and-planned-matrix-rows|planned-compatibility-matrix-rows)" "$readiness_output"
	v1_status=blocked
fi
release_readiness_status="$(awk -F= '$1 == "release_readiness_status" {print $2}' "$readiness_output")"
test -n "$release_readiness_status"

printf 'v1_acceptance_evidence_status=%s\n' "$v1_status"
printf 'v1_acceptance_compatibility_total=%s\n' "$(printf '%s\n' "$compatibility_status" | awk -F= '$1 == "compatibility_total_count" {print $2}')"
printf 'v1_acceptance_compatibility_planned=%s\n' "$(printf '%s\n' "$compatibility_status" | awk -F= '$1 == "compatibility_planned_count" {print $2}')"
printf 'v1_acceptance_release_readiness=%s\n' "$release_readiness_status"
