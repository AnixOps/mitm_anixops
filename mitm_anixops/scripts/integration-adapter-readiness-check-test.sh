#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CHECK="$ROOT/scripts/integration-adapter-readiness-check.sh"

test -s "$CHECK"

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

NETWORKCORE="$TMP_DIR/networkcore_integration.md"
OVERVIEW="$TMP_DIR/overview.md"
REPOSITORY_AUDIT="$TMP_DIR/repository-audit.md"
RELEASE_GATE="$TMP_DIR/release-gate.md"
RELEASE_DRY_RUN="$TMP_DIR/release-dry-run.md"
MAKEFILE="$TMP_DIR/Makefile"
CHECK_FILE="$TMP_DIR/check.sh"
BUILD_WORKFLOW="$TMP_DIR/build.yml"
RELEASE_WORKFLOW="$TMP_DIR/release.yml"
DRY_RUN_WORKFLOW="$TMP_DIR/release-dry-run.yml"
README="$TMP_DIR/README.md"
OUTPUT="$TMP_DIR/output.txt"

write_passing_fixture() {
	cat > "$NETWORKCORE" <<'EOF'
# NetworkCore Integration Notes

```text
networkcore-integration-alpha-baseline=v0.45.10-alpha
networkcore-integration-current-mode=vendored-policy-core-adapter
networkcore-integration-v1-boundary=adapter-owned-data-plane
networkcore-integration-live-mutation-status=deferred
integration-adapter-readiness-gate=scripts/integration-adapter-readiness-check.sh
integration-adapter-readiness-self-test=scripts/integration-adapter-readiness-check-test.sh
integration-adapter-readiness-status=ci-gated-alpha-boundary
integration-adapter-readiness-release-artifact=policy-core-runner-proxy-shim-bindings-docs
integration-adapter-readiness-production-claim=not-production-networkcore-adapter
```
EOF

	cat > "$OVERVIEW" <<'EOF'
# Architecture Overview

## Integration Adapter Readiness Gate

The gate verifies policy-core adapter evidence without claiming production
NetworkCore data-plane support.
EOF

	cat > "$REPOSITORY_AUDIT" <<'EOF'
# Repository Audit

CI includes scripts/integration-adapter-readiness-check.sh.
EOF

	cat > "$RELEASE_GATE" <<'EOF'
# Release Gate Design

ci-workflow-integration-adapter-readiness-gate=scripts/integration-adapter-readiness-check.sh
release-workflow-integration-adapter-readiness-gate=scripts/integration-adapter-readiness-check.sh
EOF

	cat > "$RELEASE_DRY_RUN" <<'EOF'
# Release Dry-Run Source Contract

release-dry-run-integration-adapter-readiness-gate=scripts/integration-adapter-readiness-check.sh
EOF

	cat > "$MAKEFILE" <<'EOF'
alpha-dist: all runner proxy-shim
	cp $(RUNNER_BIN) $(BUILD_DIR)/$(ALPHA_NAME)/bin/
	cp $(PROXY_SHIM_BIN) $(BUILD_DIR)/$(ALPHA_NAME)/bin/
	cp $(SCRIPT_RUNNER_JS) $(BUILD_DIR)/$(ALPHA_NAME)/bin/anixops-script-runner.js
	cp bindings/go/anixops/*.go $(BUILD_DIR)/$(ALPHA_NAME)/bindings/go/anixops/
	cp bindings/rust/mitm-anixops/src/*.rs $(BUILD_DIR)/$(ALPHA_NAME)/bindings/rust/mitm-anixops/src/
	cp README.md docs/networkcore_integration.md $(BUILD_DIR)/$(ALPHA_NAME)/docs/
	cp docs/architecture/*.md $(BUILD_DIR)/$(ALPHA_NAME)/docs/architecture/
	cp docs/compatibility/*.md $(BUILD_DIR)/$(ALPHA_NAME)/docs/compatibility/

proxy-shim-check:
pkg-config-check:
go-binding-check:
rust-binding-check:
cmake-package-check:
e2e:
bili-e2e:
script-contract-e2e:
bilibili-homepage-demo-e2e:
EOF

	cat > "$CHECK_FILE" <<'EOF'
#!/bin/sh
sh scripts/integration-adapter-readiness-check-test.sh
sh scripts/integration-adapter-readiness-check.sh
make proxy-shim-check
make pkg-config-check
make go-binding-check
make rust-binding-check
make cmake-package-check
make e2e
make bili-e2e
make script-contract-e2e
make bilibili-homepage-demo-e2e
EOF

	cat > "$BUILD_WORKFLOW" <<'EOF'
name: build

mitm_anixops/scripts/integration-adapter-readiness-check.sh
mitm_anixops/scripts/integration-adapter-readiness-check-test.sh
sh mitm_anixops/scripts/integration-adapter-readiness-check-test.sh
sh mitm_anixops/scripts/integration-adapter-readiness-check.sh
grep -q "integration adapter readiness check passed" mitm_anixops/scripts/integration-adapter-readiness-check.sh
grep -q "integration adapter readiness check test passed" mitm_anixops/scripts/integration-adapter-readiness-check-test.sh
EOF

	cat > "$RELEASE_WORKFLOW" <<'EOF'
name: release

sh scripts/integration-adapter-readiness-check.sh
EOF

	cat > "$DRY_RUN_WORKFLOW" <<'EOF'
name: release-dry-run

sh scripts/integration-adapter-readiness-check.sh
EOF

	printf '%s\n' "integration adapter fixture" > "$README"
}

run_gate() {
	INTEGRATION_ADAPTER_NETWORKCORE_FILE="$NETWORKCORE" \
		INTEGRATION_ADAPTER_OVERVIEW_FILE="$OVERVIEW" \
		INTEGRATION_ADAPTER_REPOSITORY_AUDIT_FILE="$REPOSITORY_AUDIT" \
		INTEGRATION_ADAPTER_RELEASE_GATE_FILE="$RELEASE_GATE" \
		INTEGRATION_ADAPTER_RELEASE_DRY_RUN_FILE="$RELEASE_DRY_RUN" \
		INTEGRATION_ADAPTER_MAKEFILE="$MAKEFILE" \
		INTEGRATION_ADAPTER_CHECK_FILE="$CHECK_FILE" \
		INTEGRATION_ADAPTER_BUILD_WORKFLOW="$BUILD_WORKFLOW" \
		INTEGRATION_ADAPTER_RELEASE_WORKFLOW="$RELEASE_WORKFLOW" \
		INTEGRATION_ADAPTER_DRY_RUN_WORKFLOW="$DRY_RUN_WORKFLOW" \
		INTEGRATION_ADAPTER_SCAN_PATHS="$README" \
		sh "$CHECK"
}

write_passing_fixture
run_gate > "$OUTPUT"
grep -q "integration adapter readiness check passed" "$OUTPUT"

write_passing_fixture
grep -v "networkcore-integration-current-mode=vendored-policy-core-adapter" "$NETWORKCORE" > "$TMP_DIR/networkcore-missing-mode.md"
mv "$TMP_DIR/networkcore-missing-mode.md" "$NETWORKCORE"
if run_gate > "$OUTPUT" 2>&1; then
	printf '%s\n' "integration adapter readiness check test failed: missing NetworkCore marker was accepted" >&2
	exit 1
fi
grep -q "missing NetworkCore adapter mode marker" "$OUTPUT"

write_passing_fixture
grep -v 'cp $(PROXY_SHIM_BIN) $(BUILD_DIR)/$(ALPHA_NAME)/bin/' "$MAKEFILE" > "$TMP_DIR/makefile-missing-proxy-shim"
mv "$TMP_DIR/makefile-missing-proxy-shim" "$MAKEFILE"
if run_gate > "$OUTPUT" 2>&1; then
	printf '%s\n' "integration adapter readiness check test failed: missing proxy shim package evidence was accepted" >&2
	exit 1
fi
grep -q "alpha-dist must package the proxy shim" "$OUTPUT"

write_passing_fixture
printf '%s\n' "Production NetworkCore adapter is supported." >> "$README"
if run_gate > "$OUTPUT" 2>&1; then
	printf '%s\n' "integration adapter readiness check test failed: forbidden production claim was accepted" >&2
	exit 1
fi
grep -q "forbidden production adapter claim found" "$OUTPUT"

printf '%s\n' "integration adapter readiness check test passed"
