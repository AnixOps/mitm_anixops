#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CHECK="$ROOT/scripts/script-runtime-security-gate.sh"

test -s "$CHECK"

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

CONTRACT="$TMP_DIR/script-runtime-common.md"
DEPENDENCY="$TMP_DIR/script-runtime-dependency.md"
REDACTION="$TMP_DIR/adapter-redaction-policy.md"
MANUAL="$TMP_DIR/manual-intervention.md"
MATRIX="$TMP_DIR/matrix.md"
SOURCE_CONTRACTS="$TMP_DIR/source-contracts.md"
CHECK_FILE="$TMP_DIR/check.sh"
README="$TMP_DIR/README.md"
OUTPUT="$TMP_DIR/output.txt"

write_passing_fixture() {
	cat > "$CONTRACT" <<'EOF'
# Script Runtime Common Source Contract

```text
script-runtime-security-boundary-policy-core-download-remote-scripts=forbidden
script-runtime-security-boundary-policy-core-expose-credentials=forbidden
script-runtime-security-boundary-production-sandbox-policy=pending-adapter-owned
script-runtime-security-boundary-production-network-apis=pending-adapter-owned
script-runtime-security-boundary-log-redaction=pending-adapter-owned
script-runtime-security-boundary-default-fail-open=required
```

CI evidence includes `scripts/script-runtime-security-gate.sh` and
`scripts/script-runtime-security-gate-test.sh`.
EOF

	cat > "$DEPENDENCY" <<'EOF'
# Script Runtime Dependency Decision

```text
script-runtime-policy-core-dependency=none
script-runtime-production-engine-selection=pending
script-runtime-new-engine-before-implementation=requires-new-source-contract-license-review-platform-review-and-github-actions-evidence
```

Any future runtime must allow no background network access unless explicitly contracted and tested.
EOF

	cat > "$REDACTION" <<'EOF'
# Adapter Redaction Policy

```text
adapter-redaction-policy-core-sensitive-payload-logging=forbidden
adapter-redaction-default-trace-export=metadata-only
adapter-redaction-runner-trace-body=not-serialized
adapter-redaction-runner-header-map=matched-policy-output-only
```
EOF

	cat > "$MANUAL" <<'EOF'
# Manual Intervention

script-runtime-engine-selection-status=pending
script-runtime-engine-selection-required-before=claiming-production-embedded-js-runtime-support
script-runtime-engine-selection-confirmation-evidence=not-yet-confirmed
script-runtime-engine-selection-current-v1-policy-core-decision=no-embedded-js-engine
adapter-redaction-implementation-status=pending
adapter-redaction-implementation-current-policy-core-decision=metadata-only-runner-trace-no-raw-body-or-full-header-map-export-by-default
EOF

	cat > "$MATRIX" <<'EOF'
| Capability | Ecosystem | Status | Source Contract | Parser Case | Positive Test | Negative Test | Compatibility Note | Unimplemented Items |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| script runtime contract | portable adapter boundary | partial | `script-runtime-common.md` | `RunnerReplay.tsv` | `make runner-check`, `scripts/script-runtime-security-gate.sh` | `scripts/script-runtime-security-gate-test.sh` | Alpha Node runner proves the adapter boundary with security markers | production runtime |
EOF

	cat > "$SOURCE_CONTRACTS" <<'EOF'
# Source Contracts

Current CI evidence:

- `scripts/script-runtime-security-gate.sh`;
- `scripts/script-runtime-security-gate-test.sh`.
EOF

	cat > "$CHECK_FILE" <<'EOF'
#!/bin/sh
sh scripts/script-runtime-security-gate-test.sh
sh scripts/script-runtime-security-gate.sh
EOF

	printf '%s\n' "script runtime security fixture" > "$README"
}

run_gate() {
	SCRIPT_RUNTIME_CONTRACT_FILE="$CONTRACT" \
		SCRIPT_RUNTIME_DEPENDENCY_FILE="$DEPENDENCY" \
		SCRIPT_RUNTIME_REDACTION_FILE="$REDACTION" \
		SCRIPT_RUNTIME_MANUAL_FILE="$MANUAL" \
		SCRIPT_RUNTIME_MATRIX_FILE="$MATRIX" \
		SCRIPT_RUNTIME_SOURCE_CONTRACTS_FILE="$SOURCE_CONTRACTS" \
		SCRIPT_RUNTIME_CHECK_FILE="$CHECK_FILE" \
		SCRIPT_RUNTIME_SECURITY_SCAN_PATHS="$README" \
		sh "$CHECK"
}

write_passing_fixture
run_gate > "$OUTPUT"
grep -q "script runtime security gate passed" "$OUTPUT"

write_passing_fixture
grep -v "script-runtime-engine-selection-status=pending" "$MANUAL" > "$TMP_DIR/manual-missing-runtime.txt"
mv "$TMP_DIR/manual-missing-runtime.txt" "$MANUAL"
if run_gate > "$OUTPUT" 2>&1; then
	printf '%s\n' "script runtime security gate test failed: missing runtime marker was accepted" >&2
	exit 1
fi
grep -q "missing pending production runtime selection marker" "$OUTPUT"

write_passing_fixture
sed 's/, `scripts\/script-runtime-security-gate.sh`//' "$MATRIX" > "$TMP_DIR/matrix-missing-gate.txt"
mv "$TMP_DIR/matrix-missing-gate.txt" "$MATRIX"
if run_gate > "$OUTPUT" 2>&1; then
	printf '%s\n' "script runtime security gate test failed: missing matrix evidence was accepted" >&2
	exit 1
fi
grep -q "matrix script runtime row must reference the security gate" "$OUTPUT"

write_passing_fixture
printf '%s\n' "Production embedded JavaScript runtime is supported." >> "$README"
if run_gate > "$OUTPUT" 2>&1; then
	printf '%s\n' "script runtime security gate test failed: forbidden runtime claim was accepted" >&2
	exit 1
fi
grep -q "forbidden script runtime security claim found" "$OUTPUT"

printf '%s\n' "script runtime security gate test passed"
