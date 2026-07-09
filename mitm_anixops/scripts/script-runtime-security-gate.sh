#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

CONTRACT=${SCRIPT_RUNTIME_CONTRACT_FILE:-"$ROOT/docs/compatibility/script-runtime-common.md"}
DEPENDENCY=${SCRIPT_RUNTIME_DEPENDENCY_FILE:-"$ROOT/docs/architecture/script-runtime-dependency.md"}
REDACTION=${SCRIPT_RUNTIME_REDACTION_FILE:-"$ROOT/docs/architecture/adapter-redaction-policy.md"}
MANUAL=${SCRIPT_RUNTIME_MANUAL_FILE:-"$ROOT/docs/manual-intervention.md"}
MATRIX=${SCRIPT_RUNTIME_MATRIX_FILE:-"$ROOT/docs/compatibility/matrix.md"}
SOURCE_CONTRACTS=${SCRIPT_RUNTIME_SOURCE_CONTRACTS_FILE:-"$ROOT/docs/compatibility/source-contracts.md"}
CHECK_SCRIPT=${SCRIPT_RUNTIME_CHECK_FILE:-"$ROOT/scripts/check.sh"}

fail() {
	printf '%s\n' "script runtime security gate failed: $1" >&2
	exit 1
}

require_file() {
	file=$1
	label=$2

	if [ ! -f "$file" ]; then
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
	"$CONTRACT:script runtime contract" \
	"$DEPENDENCY:script runtime dependency decision" \
	"$REDACTION:adapter redaction policy" \
	"$MANUAL:manual intervention register" \
	"$MATRIX:compatibility matrix" \
	"$SOURCE_CONTRACTS:compatibility source contracts" \
	"$CHECK_SCRIPT:local full check"
do
	file=${pair%%:*}
	label=${pair#*:}
	require_file "$file" "$label"
done

require_pattern "$MANUAL" "script-runtime-engine-selection-status=pending" "missing pending production runtime selection marker"
require_pattern "$MANUAL" "script-runtime-engine-selection-required-before=claiming-production-embedded-js-runtime-support" "runtime selection marker does not block production embedded JS claims"
require_pattern "$MANUAL" "script-runtime-engine-selection-confirmation-evidence=not-yet-confirmed" "runtime selection marker must remain unconfirmed"
require_pattern "$MANUAL" "script-runtime-engine-selection-current-v1-policy-core-decision=no-embedded-js-engine" "runtime selection marker lacks policy-core no-engine decision"
require_pattern "$MANUAL" "adapter-redaction-implementation-status=pending" "missing pending adapter redaction implementation marker"
require_pattern "$MANUAL" "adapter-redaction-implementation-current-policy-core-decision=metadata-only-runner-trace-no-raw-body-or-full-header-map-export-by-default" "adapter redaction marker lacks metadata-only current decision"

require_pattern "$DEPENDENCY" "script-runtime-policy-core-dependency=none" "dependency decision must keep policy core runtime dependency empty"
require_pattern "$DEPENDENCY" "script-runtime-production-engine-selection=pending" "dependency decision must keep production engine selection pending"
require_pattern "$DEPENDENCY" "script-runtime-new-engine-before-implementation=requires-new-source-contract-license-review-platform-review-and-github-actions-evidence" "dependency decision must require future review and CI evidence"
require_pattern "$DEPENDENCY" "no background network access unless explicitly contracted and tested" "dependency decision lacks network API security boundary"

require_pattern "$REDACTION" "adapter-redaction-policy-core-sensitive-payload-logging=forbidden" "redaction policy must forbid policy-core sensitive payload logging"
require_pattern "$REDACTION" "adapter-redaction-default-trace-export=metadata-only" "redaction policy must keep default trace export metadata-only"
require_pattern "$REDACTION" "adapter-redaction-runner-trace-body=not-serialized" "redaction policy must keep runner trace bodies out of trace output"
require_pattern "$REDACTION" "adapter-redaction-runner-header-map=matched-policy-output-only" "redaction policy must keep runner header maps bounded"

require_pattern "$CONTRACT" "script-runtime-security-boundary-policy-core-download-remote-scripts=forbidden" "contract must forbid policy-core remote script downloads"
require_pattern "$CONTRACT" "script-runtime-security-boundary-policy-core-expose-credentials=forbidden" "contract must forbid exposing credentials from the policy core"
require_pattern "$CONTRACT" "script-runtime-security-boundary-production-sandbox-policy=pending-adapter-owned" "contract must leave production sandbox policy adapter-owned and pending"
require_pattern "$CONTRACT" "script-runtime-security-boundary-production-network-apis=pending-adapter-owned" "contract must leave production JavaScript network APIs adapter-owned and pending"
require_pattern "$CONTRACT" "script-runtime-security-boundary-log-redaction=pending-adapter-owned" "contract must leave production log redaction adapter-owned and pending"
require_pattern "$CONTRACT" "script-runtime-security-boundary-default-fail-open=required" "contract must preserve default fail-open script behavior"
require_pattern "$CONTRACT" "scripts/script-runtime-security-gate.sh" "contract must list the script runtime security gate as CI evidence"
require_pattern "$CONTRACT" "scripts/script-runtime-security-gate-test.sh" "contract must list the script runtime security gate self-test as CI evidence"

require_pattern "$SOURCE_CONTRACTS" "scripts/script-runtime-security-gate.sh" "source contracts must list the script runtime security gate evidence"
require_pattern "$SOURCE_CONTRACTS" "scripts/script-runtime-security-gate-test.sh" "source contracts must list the script runtime security gate self-test evidence"
require_pattern "$CHECK_SCRIPT" "sh scripts/script-runtime-security-gate-test.sh" "local full check must run the script runtime security gate self-test"
require_pattern "$CHECK_SCRIPT" "sh scripts/script-runtime-security-gate.sh" "local full check must run the script runtime security gate"

script_runtime_row=$(grep -F "| script runtime contract |" "$MATRIX" || true)
row_count=$(printf '%s\n' "$script_runtime_row" | sed '/^$/d' | wc -l | tr -d ' ')
if [ "$row_count" -ne 1 ]; then
	fail "compatibility matrix must contain exactly one script runtime contract row"
fi
if ! printf '%s\n' "$script_runtime_row" | grep -F -- '`scripts/script-runtime-security-gate.sh`' >/dev/null; then
	fail "matrix script runtime row must reference the security gate"
fi
if ! printf '%s\n' "$script_runtime_row" | grep -F -- '`scripts/script-runtime-security-gate-test.sh`' >/dev/null; then
	fail "matrix script runtime row must reference the security gate self-test"
fi

scan_list=$(mktemp)
trap 'rm -f "$scan_list"' EXIT HUP INT TERM

if [ -n "${SCRIPT_RUNTIME_SECURITY_SCAN_PATHS:-}" ]; then
	printf '%s\n' "$SCRIPT_RUNTIME_SECURITY_SCAN_PATHS" | tr ':' '\n' > "$scan_list"
else
	for rel_path in README.md ROADMAP.md TODO.md CHANGELOG.md CONTRIBUTING.md docs; do
		printf '%s/%s\n' "$ROOT" "$rel_path"
	done > "$scan_list"
fi

forbidden_claims='production embedded (quickjs|javascriptcore|javascript|js) runtime (is )?(supported|implemented|enabled|ready)|policy core (downloads|fetches) remote scripts|remote script (download|fetch|cache refresh) (is )?(supported|implemented|enabled|ready) by default|script runner exposes credentials|javascript network apis are exposed by default|raw (request|response) (body|headers?) logging (is )?(enabled|supported) by default'

while IFS= read -r path; do
	[ -n "$path" ] || continue
	if [ ! -e "$path" ]; then
		fail "scan path does not exist: $path"
	fi
	if LC_ALL=C grep -RInEi -- "$forbidden_claims" "$path"; then
		fail "forbidden script runtime security claim found"
	fi
done < "$scan_list"

printf '%s\n' "script runtime security gate passed"
