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

printf '%s\n' "$required_markers" | while IFS= read -r item; do
	[ -n "$item" ] || continue
	status="$(awk -F= -v key="$item-status" '$1 == key {print $2}' "$MANUAL")"
	test -n "$status"
	case "$status" in
		pending | confirmed)
			;;
		*)
			printf 'manual intervention marker has unsupported status: %s=%s\n' "$item" "$status" >&2
			exit 1
			;;
	esac
	grep -F "$item-scope=" "$MANUAL" >/dev/null
	grep -F "$item-required-before=" "$MANUAL" >/dev/null
	evidence_count="$(awk -F= -v key="$item-confirmation-evidence" '$1 == key {count++} END {print count + 0}' "$MANUAL")"
	if [ "$evidence_count" -ne 1 ]; then
		printf 'manual intervention marker must have exactly one confirmation evidence field: %s\n' "$item" >&2
		exit 1
	fi
	evidence="$(awk -F= -v key="$item-confirmation-evidence" '$1 == key {print $2}' "$MANUAL")"
	if [ "$status" = "pending" ]; then
		if [ "$evidence" != "not-yet-confirmed" ]; then
			printf 'pending manual intervention marker must use placeholder evidence: %s\n' "$item" >&2
			exit 1
		fi
		grep -F "$item-next-action=" "$MANUAL" >/dev/null
	else
		if [ -z "$evidence" ] || [ "$evidence" = "not-yet-confirmed" ]; then
			printf 'confirmed manual intervention marker lacks confirmation evidence: %s\n' "$item" >&2
			exit 1
		fi
	fi
done

grep -F "github-remote-status=confirmed" "$MANUAL" >/dev/null
grep -F "alpha-0.45.10-release-status=confirmed" "$MANUAL" >/dev/null
grep -F "script-runtime-policy-core-dependency-decision-status=confirmed" "$MANUAL" >/dev/null

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
