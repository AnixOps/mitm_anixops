#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
MANUAL=${MANUAL_INTERVENTION_FILE:-"$ROOT/docs/manual-intervention.md"}

test -f "$MANUAL"

grep -F "## Current Pending Items" "$MANUAL" >/dev/null
grep -F "## Completed Items" "$MANUAL" >/dev/null
grep -F "## Rules" "$MANUAL" >/dev/null

required_markers='
branch-protection
protected-tags
release-environment-approval
third-party-plugin-corpus-license
script-runtime-engine-selection
ca-root-generation-policy
platform-certificate-store
protected-runtime-environment
mitm-signing-materials
windows-signing
macos-signing-notarization
ios-network-extension
certificate-trust-ux
networkcore-v1-adapter-boundary
route-selection-adapter
adapter-redaction-implementation
task-scheduler-runtime
stash-expanded-parser-support
shadowrocket-expanded-parser-support
'

completed_markers='
github-remote
alpha-0.45.10-release
script-runtime-policy-core-dependency-decision
'

field_count() {
	key=$1
	awk -F= -v key="$key" '$1 == key {count++} END {print count + 0}' "$MANUAL"
}

field_value() {
	key=$1
	awk -F= -v key="$key" '$1 == key {print $2}' "$MANUAL"
}

require_single_field() {
	item=$1
	suffix=$2
	key="${item}-${suffix}"
	count="$(field_count "$key")"
	if [ "$count" -ne 1 ]; then
		printf 'manual intervention marker must have exactly one %s field: %s\n' "$suffix" "$item" >&2
		exit 1
	fi
}

require_confirmed_evidence() {
	item=$1
	require_single_field "$item" "status"
	require_single_field "$item" "confirmation-evidence"
	status="$(field_value "$item-status")"
	if [ "$status" != "confirmed" ]; then
		printf 'manual intervention completed marker must be confirmed: %s=%s\n' "$item" "$status" >&2
		exit 1
	fi
	evidence="$(field_value "$item-confirmation-evidence")"
	if [ -z "$evidence" ] || [ "$evidence" = "not-yet-confirmed" ]; then
		printf 'confirmed manual intervention marker lacks confirmation evidence: %s\n' "$item" >&2
		exit 1
	fi
}

printf '%s\n' "$required_markers" | while IFS= read -r item; do
	[ -n "$item" ] || continue
	require_single_field "$item" "status"
	require_single_field "$item" "scope"
	require_single_field "$item" "required-before"
	require_single_field "$item" "confirmation-evidence"
	status="$(field_value "$item-status")"
	case "$status" in
		pending | confirmed)
			;;
		*)
			printf 'manual intervention marker has unsupported status: %s=%s\n' "$item" "$status" >&2
			exit 1
			;;
	esac
	evidence="$(field_value "$item-confirmation-evidence")"
	if [ "$status" = "pending" ]; then
		require_single_field "$item" "next-action"
		if [ "$evidence" != "not-yet-confirmed" ]; then
			printf 'pending manual intervention marker must use placeholder evidence: %s\n' "$item" >&2
			exit 1
		fi
	else
		if [ -z "$evidence" ] || [ "$evidence" = "not-yet-confirmed" ]; then
			printf 'confirmed manual intervention marker lacks confirmation evidence: %s\n' "$item" >&2
			exit 1
		fi
	fi
done

printf '%s\n' "$completed_markers" | while IFS= read -r item; do
	[ -n "$item" ] || continue
	require_confirmed_evidence "$item"
done

awk -F= '
/-status=/ {
	if ($2 != "pending" && $2 != "confirmed") {
		printf "manual intervention marker has unsupported status: %s\n", $0 > "/dev/stderr"
		failed = 1
	}
}
END {
	if (failed) {
		exit 1
	}
}
' "$MANUAL"

pending_count=$(awk -F= '$1 ~ /-status$/ && $2 == "pending" {count++} END {print count + 0}' "$MANUAL")
confirmed_count=$(awk -F= '$1 ~ /-status$/ && $2 == "confirmed" {count++} END {print count + 0}' "$MANUAL")
marker_count=$((pending_count + confirmed_count))
if [ "$marker_count" -lt 10 ]; then
	printf '%s\n' "manual intervention check failed: too few manual markers" >&2
	exit 1
fi

printf 'manual intervention check passed (%s pending markers, %s confirmed markers)\n' "$pending_count" "$confirmed_count"
