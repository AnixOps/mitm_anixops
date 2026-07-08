#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
MANUAL="$ROOT/docs/manual-intervention.md"

test -s "$MANUAL"

confirmed_fixture=$(mktemp)
all_confirmed_fixture=$(mktemp)
missing_pending_evidence_fixture=$(mktemp)
missing_pending_evidence_output=$(mktemp)
missing_evidence_fixture=$(mktemp)
missing_evidence_output=$(mktemp)
trap 'rm -f "$confirmed_fixture" "$all_confirmed_fixture" "$missing_pending_evidence_fixture" "$missing_pending_evidence_output" "$missing_evidence_fixture" "$missing_evidence_output"' EXIT HUP INT TERM

sed \
	-e 's/^branch-protection-status=pending$/branch-protection-status=confirmed/' \
	-e 's/^branch-protection-confirmation-evidence=not-yet-confirmed$/branch-protection-confirmation-evidence=redacted-github-ruleset-audit-note/' \
	"$MANUAL" > "$confirmed_fixture"

MANUAL_INTERVENTION_FILE="$confirmed_fixture" sh "$ROOT/scripts/manual-intervention-check.sh" >/dev/null

sed \
	-e 's/-status=pending$/-status=confirmed/' \
	-e 's/-confirmation-evidence=not-yet-confirmed$/-confirmation-evidence=redacted-manual-gate-audit-note/' \
	"$MANUAL" > "$all_confirmed_fixture"

MANUAL_INTERVENTION_FILE="$all_confirmed_fixture" sh "$ROOT/scripts/manual-intervention-check.sh" >/dev/null

sed \
	-e '/^branch-protection-confirmation-evidence=/d' \
	"$MANUAL" > "$missing_pending_evidence_fixture"

if MANUAL_INTERVENTION_FILE="$missing_pending_evidence_fixture" sh "$ROOT/scripts/manual-intervention-check.sh" > "$missing_pending_evidence_output" 2>&1; then
	printf '%s\n' "manual intervention transition check failed: pending marker without placeholder evidence passed" >&2
	exit 1
fi

grep -F "manual intervention marker must have exactly one confirmation evidence field: branch-protection" "$missing_pending_evidence_output" >/dev/null

sed \
	-e 's/^branch-protection-status=pending$/branch-protection-status=confirmed/' \
	"$MANUAL" > "$missing_evidence_fixture"

if MANUAL_INTERVENTION_FILE="$missing_evidence_fixture" sh "$ROOT/scripts/manual-intervention-check.sh" > "$missing_evidence_output" 2>&1; then
	printf '%s\n' "manual intervention transition check failed: confirmed marker without evidence passed" >&2
	exit 1
fi

grep -F "confirmed manual intervention marker lacks confirmation evidence: branch-protection" "$missing_evidence_output" >/dev/null

printf '%s\n' "manual intervention transition check passed"
