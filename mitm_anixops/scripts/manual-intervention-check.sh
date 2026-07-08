#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
MANUAL="$ROOT/docs/manual-intervention.md"

test -f "$MANUAL"

grep -F "## Current Pending Items" "$MANUAL" >/dev/null
grep -F "## Completed Items" "$MANUAL" >/dev/null
grep -F "## Rules" "$MANUAL" >/dev/null

required_pending='
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

printf '%s\n' "$required_pending" | while IFS= read -r item; do
	[ -n "$item" ] || continue
	grep -F "$item-status=pending" "$MANUAL" >/dev/null
	grep -F "$item-scope=" "$MANUAL" >/dev/null
	grep -F "$item-required-before=" "$MANUAL" >/dev/null
	grep -F "$item-next-action=" "$MANUAL" >/dev/null
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

pending_count=$(grep -c -- '-status=pending' "$MANUAL")
if [ "$pending_count" -lt 10 ]; then
	printf '%s\n' "manual intervention check failed: too few pending markers" >&2
	exit 1
fi

printf 'manual intervention check passed (%s pending markers)\n' "$pending_count"
