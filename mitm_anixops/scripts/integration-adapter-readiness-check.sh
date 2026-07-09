#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
REPO=$(CDPATH= cd -- "$ROOT/.." && pwd)

NETWORKCORE=${INTEGRATION_ADAPTER_NETWORKCORE_FILE:-"$ROOT/docs/networkcore_integration.md"}
OVERVIEW=${INTEGRATION_ADAPTER_OVERVIEW_FILE:-"$ROOT/docs/architecture/overview.md"}
REPOSITORY_AUDIT=${INTEGRATION_ADAPTER_REPOSITORY_AUDIT_FILE:-"$ROOT/docs/architecture/repository-audit.md"}
RELEASE_GATE=${INTEGRATION_ADAPTER_RELEASE_GATE_FILE:-"$ROOT/docs/architecture/release-gate.md"}
RELEASE_DRY_RUN=${INTEGRATION_ADAPTER_RELEASE_DRY_RUN_FILE:-"$ROOT/docs/architecture/release-dry-run.md"}
MAKEFILE=${INTEGRATION_ADAPTER_MAKEFILE:-"$ROOT/Makefile"}
CHECK_SCRIPT=${INTEGRATION_ADAPTER_CHECK_FILE:-"$ROOT/scripts/check.sh"}
BUILD_WORKFLOW=${INTEGRATION_ADAPTER_BUILD_WORKFLOW:-"$REPO/.github/workflows/build.yml"}
RELEASE_WORKFLOW=${INTEGRATION_ADAPTER_RELEASE_WORKFLOW:-"$REPO/.github/workflows/release.yml"}
DRY_RUN_WORKFLOW=${INTEGRATION_ADAPTER_DRY_RUN_WORKFLOW:-"$REPO/.github/workflows/release-dry-run.yml"}

fail() {
	printf '%s\n' "integration adapter readiness check failed: $1" >&2
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
	"$NETWORKCORE:NetworkCore integration notes" \
	"$OVERVIEW:architecture overview" \
	"$REPOSITORY_AUDIT:repository audit" \
	"$RELEASE_GATE:release gate contract" \
	"$RELEASE_DRY_RUN:release dry-run contract" \
	"$MAKEFILE:Makefile" \
	"$CHECK_SCRIPT:local full check" \
	"$BUILD_WORKFLOW:build workflow" \
	"$RELEASE_WORKFLOW:release workflow" \
	"$DRY_RUN_WORKFLOW:release dry-run workflow"
do
	file=${pair%%:*}
	label=${pair#*:}
	require_file "$file" "$label"
done

require_pattern "$NETWORKCORE" "networkcore-integration-alpha-baseline=v0.45.10-alpha" "missing NetworkCore alpha baseline marker"
require_pattern "$NETWORKCORE" "networkcore-integration-current-mode=vendored-policy-core-adapter" "missing NetworkCore adapter mode marker"
require_pattern "$NETWORKCORE" "networkcore-integration-v1-boundary=adapter-owned-data-plane" "missing NetworkCore v1 boundary marker"
require_pattern "$NETWORKCORE" "networkcore-integration-live-mutation-status=deferred" "missing deferred live mutation marker"
require_pattern "$NETWORKCORE" "integration-adapter-readiness-gate=scripts/integration-adapter-readiness-check.sh" "missing integration adapter readiness gate marker"
require_pattern "$NETWORKCORE" "integration-adapter-readiness-self-test=scripts/integration-adapter-readiness-check-test.sh" "missing integration adapter readiness self-test marker"
require_pattern "$NETWORKCORE" "integration-adapter-readiness-status=ci-gated-alpha-boundary" "missing integration adapter readiness status marker"
require_pattern "$NETWORKCORE" "integration-adapter-readiness-release-artifact=policy-core-runner-proxy-shim-bindings-docs" "missing integration adapter release artifact marker"
require_pattern "$NETWORKCORE" "integration-adapter-readiness-production-claim=not-production-networkcore-adapter" "missing integration adapter production-claim boundary"

require_pattern "$OVERVIEW" "Integration Adapter Readiness Gate" "architecture overview must mention the integration adapter readiness gate"
require_pattern "$REPOSITORY_AUDIT" "integration-adapter-readiness-check.sh" "repository audit must mention the integration adapter readiness gate"
require_pattern "$RELEASE_GATE" "ci-workflow-integration-adapter-readiness-gate=scripts/integration-adapter-readiness-check.sh" "release gate missing CI integration adapter readiness marker"
require_pattern "$RELEASE_GATE" "release-workflow-integration-adapter-readiness-gate=scripts/integration-adapter-readiness-check.sh" "release gate missing release integration adapter readiness marker"
require_pattern "$RELEASE_DRY_RUN" "release-dry-run-integration-adapter-readiness-gate=scripts/integration-adapter-readiness-check.sh" "release dry-run missing integration adapter readiness marker"

for target in \
	proxy-shim-check \
	go-binding-check \
	rust-binding-check \
	cmake-package-check \
	pkg-config-check \
	e2e \
	bili-e2e \
	script-contract-e2e \
	bilibili-homepage-demo-e2e \
	alpha-dist
do
	require_pattern "$MAKEFILE" "$target:" "Makefile missing target: $target"
done

require_pattern "$MAKEFILE" "alpha-dist: all runner proxy-shim" "alpha-dist must depend on runner and proxy shim"
require_pattern "$MAKEFILE" 'cp $(RUNNER_BIN) $(BUILD_DIR)/$(ALPHA_NAME)/bin/' "alpha-dist must package the C runner"
require_pattern "$MAKEFILE" 'cp $(PROXY_SHIM_BIN) $(BUILD_DIR)/$(ALPHA_NAME)/bin/' "alpha-dist must package the proxy shim"
require_pattern "$MAKEFILE" 'cp $(SCRIPT_RUNNER_JS) $(BUILD_DIR)/$(ALPHA_NAME)/bin/anixops-script-runner.js' "alpha-dist must package the script runner"
require_pattern "$MAKEFILE" 'cp bindings/go/anixops/*.go $(BUILD_DIR)/$(ALPHA_NAME)/bindings/go/anixops/' "alpha-dist must package Go binding sources"
require_pattern "$MAKEFILE" 'cp bindings/rust/mitm-anixops/src/*.rs $(BUILD_DIR)/$(ALPHA_NAME)/bindings/rust/mitm-anixops/src/' "alpha-dist must package Rust binding sources"
require_pattern "$MAKEFILE" 'docs/networkcore_integration.md $(BUILD_DIR)/$(ALPHA_NAME)/docs/' "alpha-dist must package NetworkCore integration docs"
require_pattern "$MAKEFILE" 'cp docs/architecture/*.md $(BUILD_DIR)/$(ALPHA_NAME)/docs/architecture/' "alpha-dist must package architecture docs"
require_pattern "$MAKEFILE" 'cp docs/compatibility/*.md $(BUILD_DIR)/$(ALPHA_NAME)/docs/compatibility/' "alpha-dist must package compatibility docs"

for command in \
	"sh scripts/integration-adapter-readiness-check-test.sh" \
	"sh scripts/integration-adapter-readiness-check.sh" \
	"make proxy-shim-check" \
	"make pkg-config-check" \
	"make go-binding-check" \
	"make rust-binding-check" \
	"make cmake-package-check" \
	"make e2e" \
	"make bili-e2e" \
	"make script-contract-e2e" \
	"make bilibili-homepage-demo-e2e"
do
	require_pattern "$CHECK_SCRIPT" "$command" "local full check missing command: $command"
done

require_pattern "$BUILD_WORKFLOW" "mitm_anixops/scripts/integration-adapter-readiness-check.sh" "build workflow missing integration adapter readiness script"
require_pattern "$BUILD_WORKFLOW" "mitm_anixops/scripts/integration-adapter-readiness-check-test.sh" "build workflow missing integration adapter readiness self-test"
require_pattern "$BUILD_WORKFLOW" "sh mitm_anixops/scripts/integration-adapter-readiness-check-test.sh" "build workflow must run integration adapter readiness self-test"
require_pattern "$BUILD_WORKFLOW" "sh mitm_anixops/scripts/integration-adapter-readiness-check.sh" "build workflow must run integration adapter readiness gate"
require_pattern "$BUILD_WORKFLOW" "integration adapter readiness check passed" "build workflow must statically prove the readiness gate success marker"
require_pattern "$BUILD_WORKFLOW" "integration adapter readiness check test passed" "build workflow must statically prove the readiness self-test success marker"
require_pattern "$DRY_RUN_WORKFLOW" "sh scripts/integration-adapter-readiness-check.sh" "release dry-run must run integration adapter readiness gate"
require_pattern "$RELEASE_WORKFLOW" "sh scripts/integration-adapter-readiness-check.sh" "release workflow must run integration adapter readiness gate"

scan_list=$(mktemp)
trap 'rm -f "$scan_list"' EXIT HUP INT TERM

if [ -n "${INTEGRATION_ADAPTER_SCAN_PATHS:-}" ]; then
	printf '%s\n' "$INTEGRATION_ADAPTER_SCAN_PATHS" | tr ':' '\n' > "$scan_list"
else
	for rel_path in README.md ROADMAP.md TODO.md CHANGELOG.md CONTRIBUTING.md docs; do
		printf '%s/%s\n' "$ROOT" "$rel_path"
	done > "$scan_list"
fi

forbidden_claims='production networkcore adapter (is )?(supported|implemented|enabled|ready)|networkcore production adapter (is )?(supported|implemented|enabled|ready)|v1\.2\.0 production networkcore adapter|adapter readiness gate proves production data plane|policy core owns (the )?production data plane|mitm_anixops owns (the )?networkcore data plane'

while IFS= read -r path; do
	[ -n "$path" ] || continue
	if [ ! -e "$path" ]; then
		fail "scan path does not exist: $path"
	fi
	if LC_ALL=C grep -RInEi -- "$forbidden_claims" "$path"; then
		fail "forbidden production adapter claim found"
	fi
done < "$scan_list"

printf '%s\n' "integration adapter readiness check passed"
