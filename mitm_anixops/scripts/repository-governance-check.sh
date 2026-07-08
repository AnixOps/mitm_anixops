#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
REPO=$(CDPATH= cd -- "$ROOT/.." && pwd)

DOC="$ROOT/docs/architecture/repository-governance.md"
MANUAL="$ROOT/docs/manual-intervention.md"
RELEASE_WORKFLOW="$REPO/.github/workflows/release.yml"

test -s "$DOC"
test -s "$MANUAL"
test -s "$RELEASE_WORKFLOW"

grep -q "repository-governance-contract-status=accepted" "$DOC"
grep -q "repository-governance-automation-mode=static-contract-and-manual-confirmation" "$DOC"
grep -q "repository-governance-branch-protection=manual-confirmation-required" "$DOC"
grep -q "repository-governance-protected-tags=manual-confirmation-required" "$DOC"
grep -q "repository-governance-release-environment=manual-confirmation-required" "$DOC"
grep -q "repository-governance-v1-publication-status=blocked-until-manual-confirmation" "$DOC"

blocking_markers=""
for marker in branch-protection protected-tags release-environment-approval; do
	status="$(awk -F= -v key="${marker}-status" '$1 == key {print $2}' "$MANUAL")"
	evidence="$(awk -F= -v key="${marker}-confirmation-evidence" '$1 == key {print $2}' "$MANUAL")"
	test -n "$status"
	test -n "$evidence"
	grep -F "${marker}-scope=" "$MANUAL" >/dev/null
	grep -F "${marker}-required-before=" "$MANUAL" >/dev/null
	grep -F "${marker}-next-action=" "$MANUAL" >/dev/null

	case "$status" in
		pending)
			if [ -n "$blocking_markers" ]; then
				blocking_markers="${blocking_markers},${marker}"
			else
				blocking_markers="$marker"
			fi
			;;
		confirmed)
			if [ "$evidence" = "not-yet-confirmed" ]; then
				printf '%s\n' "confirmed governance marker lacks evidence: ${marker}" >&2
				exit 1
			fi
			;;
		*)
			printf '%s\n' "unsupported governance marker status: ${marker}=${status}" >&2
			exit 1
			;;
	esac
done

grep -q "environment: github-release-publication" "$RELEASE_WORKFLOW"
grep -q "same-commit-main-build-success" "$RELEASE_WORKFLOW"
grep -q "release-readiness-check.sh" "$RELEASE_WORKFLOW"
grep -q "stable release readiness did not pass" "$RELEASE_WORKFLOW"
grep -q "gh release view" "$RELEASE_WORKFLOW"
grep -q "gh release create" "$RELEASE_WORKFLOW"

if [ -n "$blocking_markers" ]; then
	printf 'repository_governance_status=blocked\n'
	printf 'repository_governance_blocking_markers=%s\n' "$blocking_markers"
else
	printf 'repository_governance_status=passed\n'
	printf 'repository_governance_blocking_markers=none\n'
fi
