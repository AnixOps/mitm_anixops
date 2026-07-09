#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
REPO=$(CDPATH= cd -- "$ROOT/.." && pwd)

DRY_RUN_WORKFLOW="$REPO/.github/workflows/release-dry-run.yml"
RELEASE_WORKFLOW="$REPO/.github/workflows/release.yml"
RELEASE_DRY_RUN_DOC="$ROOT/docs/architecture/release-dry-run.md"
RELEASE_GATE_DOC="$ROOT/docs/architecture/release-gate.md"
SENSITIVE_MATERIAL_CHECK="$ROOT/scripts/release-sensitive-material-check.sh"
SENSITIVE_MATERIAL_TEST="$ROOT/scripts/release-sensitive-material-check-test.sh"

for file in \
	"$DRY_RUN_WORKFLOW" \
	"$RELEASE_WORKFLOW" \
	"$RELEASE_DRY_RUN_DOC" \
	"$RELEASE_GATE_DOC" \
	"$SENSITIVE_MATERIAL_CHECK" \
	"$SENSITIVE_MATERIAL_TEST"
do
	test -s "$file"
done

require_pattern() {
	file=$1
	pattern=$2
	message=$3

	if ! grep -F "$pattern" "$file" >/dev/null; then
		printf '%s: %s\n' "$message" "$pattern" >&2
		exit 1
	fi
}

require_regex() {
	file=$1
	pattern=$2
	message=$3

	if ! grep -Eq "$pattern" "$file"; then
		printf '%s: %s\n' "$message" "$pattern" >&2
		exit 1
	fi
}

require_absent() {
	file=$1
	pattern=$2
	message=$3

	if grep -F "$pattern" "$file" >/dev/null; then
		printf '%s: %s\n' "$message" "$pattern" >&2
		exit 1
	fi
}

compatibility_count_keys='
compatibility_supported_count
compatibility_partial_count
compatibility_planned_count
compatibility_unsupported_count
compatibility_total_count
'

check_compatibility_summary_keys() {
	file=$1
	label=$2

	keys="$(
		awk '
		/Summarize compatibility matrix status/ {
			seen = 1
		}
		seen && /for key in \\/ {
			capture = 1
			next
		}
		capture && /^[[:space:]]*do[[:space:]]*$/ {
			exit
		}
		capture {
			line = $0
			gsub(/\\/, "", line)
			sub(/^[[:space:]]*/, "", line)
			sub(/[[:space:]]*$/, "", line)
			if (line != "") {
				print line
			}
		}
		' "$file"
	)"

	for key in $compatibility_count_keys; do
		count="$(printf '%s\n' "$keys" | awk -v key="$key" '$0 == key {count++} END {print count + 0}')"
		if [ "$count" -ne 1 ]; then
			printf '%s compatibility summary key count mismatch: %s=%s\n' "$label" "$key" "$count" >&2
			exit 1
		fi
	done

	key_count="$(printf '%s\n' "$keys" | awk 'NF {count++} END {print count + 0}')"
	if [ "$key_count" -ne 5 ]; then
		printf '%s compatibility summary must contain exactly 5 keys, got %s\n' "$label" "$key_count" >&2
		exit 1
	fi

	unexpected_key="$(
		printf '%s\n' "$keys" |
			awk '$0 !~ /^compatibility_(supported|partial|planned|unsupported|total)_count$/ {print; exit}'
	)"
	if [ -n "$unexpected_key" ]; then
		printf '%s compatibility summary has unexpected key: %s\n' "$label" "$unexpected_key" >&2
		exit 1
	fi
}

check_common_workflow_metadata() {
	file=$1
	label=$2

	require_pattern "$file" "Summarize compatibility matrix status" "$label missing compatibility summary step"
	require_pattern "$file" "scripts/compatibility-status-summary.sh" "$label missing compatibility status helper"
	require_pattern "$file" "scripts/compatibility-status-summary-check.sh" "$label missing compatibility status summary check"
	check_compatibility_summary_keys "$file" "$label"
	require_pattern "$file" "scripts/manual-intervention-check.sh" "$label missing manual-intervention evidence gate"
	require_pattern "$file" "scripts/manual-intervention-transition-check.sh" "$label missing manual-intervention transition gate"
	require_pattern "$file" "scripts/release-checklist-check.sh" "$label missing release checklist gate"
	require_pattern "$file" "scripts/release-metadata-check.sh" "$label missing release metadata gate"
	require_pattern "$file" "scripts/release-sensitive-material-check.sh" "$label missing release sensitive material gate"
	require_pattern "$file" "release-readiness-check.sh" "$label missing release readiness gate"
	require_pattern "$file" "sha256sum" "$label missing SHA-256 generation"
	require_pattern "$file" "\"release_version\"" "$label manifest missing release version"
	require_pattern "$file" "\"manifest_schema_version\"" "$label manifest missing schema version"
	require_pattern "$file" "\"artifact_digest_algorithm\"" "$label manifest missing artifact digest algorithm"
	require_pattern "$file" "\"checksum_sidecar_format\"" "$label manifest missing checksum sidecar format"
	require_pattern "$file" "\"release_commit\"" "$label manifest missing release commit"
	require_pattern "$file" "\"source_mode\"" "$label manifest missing source mode"
	require_pattern "$file" "\"release_workflow_run_id\"" "$label manifest missing release workflow run id"
	require_pattern "$file" "\"release_workflow_run_url\"" "$label manifest missing release workflow run url"
	require_pattern "$file" "\"ci_run_id\"" "$label manifest missing CI run id"
	require_pattern "$file" "\"ci_run_url\"" "$label manifest missing CI run url"
	require_pattern "$file" "\"ci_run_conclusion\"" "$label manifest missing CI run conclusion"
	require_pattern "$file" "\"publication_gate\"" "$label manifest missing publication gate"
	require_pattern "$file" "\"release_readiness_status\"" "$label manifest missing readiness status"
	require_pattern "$file" "\"release_readiness_blocking_reason\"" "$label manifest missing readiness reason"
	require_pattern "$file" "\"adapter_readiness_status\"" "$label manifest missing adapter readiness status"
	require_pattern "$file" "\"adapter_readiness_gate\"" "$label manifest missing adapter readiness gate"
	require_pattern "$file" "\"adapter_readiness_scope\"" "$label manifest missing adapter readiness scope"
	require_pattern "$file" "\"adapter_readiness_production_claim\"" "$label manifest missing adapter readiness production claim"
	require_pattern "$file" "\"compatibility_scope\"" "$label manifest missing compatibility scope"
	require_pattern "$file" "\"artifact_count\"" "$label manifest missing artifact count"
	require_pattern "$file" "\"artifact_platforms\"" "$label manifest missing artifact platforms"
	require_pattern "$file" "\"artifacts\"" "$label manifest missing artifact list"
	require_pattern "$file" "\"platform\": \"linux-x64\"" "$label manifest missing Linux platform"
	require_pattern "$file" "\"platform\": \"windows-x64\"" "$label manifest missing Windows platform"
	require_pattern "$file" "\"sha256\"" "$label manifest missing artifact checksum values"
	require_pattern "$file" "\"sha256_file\"" "$label manifest missing checksum file values"
	require_pattern "$file" "manifest_sha256_file" "$label missing manifest checksum sidecar"
	require_pattern "$file" "release_notes_path" "$label missing release notes output"
	require_pattern "$file" "Source mode:" "$label notes missing source mode"
	require_pattern "$file" "Publication gate:" "$label notes missing publication gate"
	require_pattern "$file" "CI run:" "$label notes missing CI run evidence"
	require_pattern "$file" "Release workflow run:" "$label notes missing release workflow run"
	require_pattern "$file" "Manifest schema:" "$label notes missing manifest schema"
	require_pattern "$file" "Artifact digest algorithm:" "$label notes missing artifact digest algorithm"
	require_pattern "$file" "Compatibility matrix status counts:" "$label notes missing compatibility counts"
	require_pattern "$file" "Adapter readiness:" "$label notes missing adapter readiness status"
	require_pattern "$file" "Adapter readiness scope:" "$label notes missing adapter readiness scope"
	require_pattern "$file" "Compatibility scope:" "$label notes missing compatibility scope"
	require_pattern "$file" "Artifact count:" "$label notes missing artifact count"
	require_pattern "$file" "Known gaps:" "$label notes missing known gaps"
	require_pattern "$file" "Rollback path:" "$label notes missing rollback path"
	require_pattern "$file" "actions/upload-artifact@v7" "$label missing workflow artifact upload"

	for key in \
		compatibility_supported_count \
		compatibility_partial_count \
		compatibility_planned_count \
		compatibility_unsupported_count \
		compatibility_total_count
	do
		require_pattern "$file" "$key" "$label missing compatibility count"
	done

	for key in \
		artifact_name \
		artifact_path \
		artifact_sha256 \
		artifact_sha256_file \
		artifact_count \
		artifact_platforms \
		windows_artifact_name \
		windows_artifact_path \
		windows_artifact_sha256 \
		windows_artifact_sha256_file \
		manifest_name \
		manifest_path \
		manifest_sha256 \
		manifest_sha256_file \
		manifest_schema_version \
		artifact_digest_algorithm \
		checksum_sidecar_format \
		source_mode \
		ci_run_id \
		ci_run_url \
		ci_run_conclusion \
		release_workflow_run_id \
		release_workflow_run_url \
		publication_gate
	do
		require_pattern "$file" "$key" "$label missing artifact metadata output"
	done

	for key in \
		adapter_readiness_status \
		adapter_readiness_gate \
		adapter_readiness_scope \
		adapter_readiness_production_claim
	do
		require_pattern "$file" "$key" "$label missing adapter readiness output"
	done

	require_regex "$file" 'grep -Eq "\^\[0-9a-f\]\{64\}  .*\$"' "$label missing SHA-256 sidecar validation"
}

check_common_workflow_metadata "$DRY_RUN_WORKFLOW" "release dry-run workflow"
check_common_workflow_metadata "$RELEASE_WORKFLOW" "release workflow"

require_pattern "$DRY_RUN_WORKFLOW" "publication=blocked" "dry-run workflow must remain non-publishing"
require_pattern "$DRY_RUN_WORKFLOW" "source_mode=\"manual-main-dry-run\"" "dry-run workflow missing manual source mode"
require_pattern "$DRY_RUN_WORKFLOW" "source_mode=\"pull-request-main-dry-run\"" "dry-run workflow missing pull-request source mode"
require_pattern "$DRY_RUN_WORKFLOW" "source_mode=\"main-push-dry-run\"" "dry-run workflow missing main-push source mode"
require_pattern "$DRY_RUN_WORKFLOW" "publication_gate=dry-run-nonpublishing-policy-and-workflow-artifacts-only" "dry-run workflow missing publication gate"
require_pattern "$DRY_RUN_WORKFLOW" "publish_eligibility_status=blocked" "dry-run workflow must report blocked publication"
require_pattern "$DRY_RUN_WORKFLOW" "publish_blocking_reason=dry-run-does-not-publish" "dry-run workflow missing dry-run publish blocker"
require_pattern "$DRY_RUN_WORKFLOW" "manual_intervention_status=pending-items-block-publication" "dry-run workflow missing manual intervention status"
require_pattern "$DRY_RUN_WORKFLOW" "Dry-run artifacts:" "dry-run notes missing artifact section"
require_pattern "$DRY_RUN_WORKFLOW" "Manual intervention:" "dry-run notes missing manual intervention section"
require_absent "$DRY_RUN_WORKFLOW" "gh release create" "release dry-run must not publish GitHub releases"

require_pattern "$RELEASE_WORKFLOW" "same-commit-main-build-success" "release workflow missing same-commit CI gate"
require_pattern "$RELEASE_WORKFLOW" "publication_gate=same-commit-ci-release-metadata-and-github-release-publication-environment" "release workflow missing tag publication gate"
require_pattern "$RELEASE_WORKFLOW" "publish_eligibility_status=\"eligible\"" "release workflow missing tag publish eligibility"
require_pattern "$RELEASE_WORKFLOW" "publish_blocking_reason=\"same-commit-ci-and-release-metadata-gates-passed\"" "release workflow missing publish gate reason"
require_pattern "$RELEASE_WORKFLOW" "environment: github-release-publication" "release workflow missing protected publication environment"
require_pattern "$RELEASE_WORKFLOW" "GH_REPO: \${{ github.repository }}" "release workflow missing explicit gh repository context"
require_pattern "$RELEASE_WORKFLOW" "gh release view" "release workflow missing immutable-release existence check"
require_pattern "$RELEASE_WORKFLOW" "gh release create" "release workflow missing GitHub Release publication"
require_pattern "$RELEASE_WORKFLOW" "stable release readiness did not pass" "release workflow missing stable readiness publication guard"
require_pattern "$RELEASE_WORKFLOW" "supersedes the tag-only v1.0.0 publication attempt" "release workflow missing replacement release note"
require_pattern "$RELEASE_WORKFLOW" "Release artifacts:" "release notes missing artifact section"
require_pattern "$RELEASE_WORKFLOW" "source_mode" "release workflow missing source mode metadata"

require_pattern "$RELEASE_DRY_RUN_DOC" "release-dry-run-publication=blocked" "dry-run contract missing blocked publication marker"
require_pattern "$RELEASE_DRY_RUN_DOC" "release-dry-run-source-mode=manual-main-push-or-pr-dry-run" "dry-run contract missing source mode marker"
require_pattern "$RELEASE_DRY_RUN_DOC" "release-dry-run-publication-gate=dry-run-nonpublishing-policy-and-workflow-artifacts-only" "dry-run contract missing publication gate marker"
require_pattern "$RELEASE_DRY_RUN_DOC" "release-dry-run-compatibility-summary=status-counts-in-manifest-notes-summary" "dry-run contract missing compatibility summary marker"
require_pattern "$RELEASE_DRY_RUN_DOC" "release-dry-run-manifest-schema=release-manifest-v1" "dry-run contract missing manifest schema marker"
require_pattern "$RELEASE_DRY_RUN_DOC" "release-dry-run-digest-format=sha256-sidecars" "dry-run contract missing digest format marker"
require_pattern "$RELEASE_DRY_RUN_DOC" "release-dry-run-workflow-run-evidence=run-id-url-in-manifest-notes-summary" "dry-run contract missing workflow run evidence marker"
require_pattern "$RELEASE_DRY_RUN_DOC" "release-dry-run-ci-run-evidence=ci-run-id-url-conclusion-in-manifest-notes-summary" "dry-run contract missing CI run evidence marker"
require_pattern "$RELEASE_DRY_RUN_DOC" "release-dry-run-artifact-evidence=artifact-count-and-platforms-in-manifest-notes-summary" "dry-run contract missing artifact evidence marker"
require_pattern "$RELEASE_DRY_RUN_DOC" "release-dry-run-adapter-readiness-manifest=ci-gated-alpha-boundary-fields" "dry-run contract missing adapter readiness manifest marker"
require_pattern "$RELEASE_DRY_RUN_DOC" "release-dry-run-compatibility-summary-static-check=scripts/compatibility-status-summary-check.sh" "dry-run contract missing compatibility summary static check marker"
require_pattern "$RELEASE_DRY_RUN_DOC" "release-dry-run-manual-intervention-static-check=scripts/manual-intervention-check.sh" "dry-run contract missing manual intervention static check marker"
require_pattern "$RELEASE_DRY_RUN_DOC" "release-dry-run-manual-intervention-transition-check=scripts/manual-intervention-transition-check.sh" "dry-run contract missing manual intervention transition check marker"
require_pattern "$RELEASE_DRY_RUN_DOC" "release-dry-run-release-checklist-static-check=scripts/release-checklist-check.sh" "dry-run contract missing release checklist static check marker"
require_pattern "$RELEASE_DRY_RUN_DOC" "release-dry-run-sensitive-material-gate=scripts/release-sensitive-material-check.sh" "dry-run contract missing sensitive material gate marker"
require_pattern "$RELEASE_DRY_RUN_DOC" "manual_intervention_status" "dry-run contract missing manual intervention output"
require_pattern "$RELEASE_DRY_RUN_DOC" "publish_eligibility_status" "dry-run contract missing publish eligibility output"
require_pattern "$RELEASE_DRY_RUN_DOC" "manifest_schema_version" "dry-run contract missing manifest schema output"
require_pattern "$RELEASE_DRY_RUN_DOC" "artifact_digest_algorithm" "dry-run contract missing artifact digest algorithm output"
require_pattern "$RELEASE_DRY_RUN_DOC" "checksum_sidecar_format" "dry-run contract missing checksum sidecar format output"
require_pattern "$RELEASE_DRY_RUN_DOC" "source_mode" "dry-run contract missing source mode output"
require_pattern "$RELEASE_DRY_RUN_DOC" "release_workflow_run_id" "dry-run contract missing release workflow run id output"
require_pattern "$RELEASE_DRY_RUN_DOC" "release_workflow_run_url" "dry-run contract missing release workflow run url output"
require_pattern "$RELEASE_DRY_RUN_DOC" "ci_run_id" "dry-run contract missing CI run id output"
require_pattern "$RELEASE_DRY_RUN_DOC" "ci_run_url" "dry-run contract missing CI run url output"
require_pattern "$RELEASE_DRY_RUN_DOC" "ci_run_conclusion" "dry-run contract missing CI run conclusion output"
require_pattern "$RELEASE_DRY_RUN_DOC" "artifact_count" "dry-run contract missing artifact count output"
require_pattern "$RELEASE_DRY_RUN_DOC" "artifact_platforms" "dry-run contract missing artifact platforms output"
require_pattern "$RELEASE_DRY_RUN_DOC" "publication_gate" "dry-run contract missing publication gate output"
require_pattern "$RELEASE_DRY_RUN_DOC" "adapter_readiness_status" "dry-run contract missing adapter readiness status output"
require_pattern "$RELEASE_DRY_RUN_DOC" "adapter_readiness_gate" "dry-run contract missing adapter readiness gate output"
require_pattern "$RELEASE_DRY_RUN_DOC" "adapter_readiness_scope" "dry-run contract missing adapter readiness scope output"
require_pattern "$RELEASE_DRY_RUN_DOC" "adapter_readiness_production_claim" "dry-run contract missing adapter readiness production output"
require_pattern "$RELEASE_DRY_RUN_DOC" "manifest, notes, or summary omit compatibility status counts" "dry-run contract missing metadata failure rule"
require_pattern "$RELEASE_DRY_RUN_DOC" "manifest, notes, or summary omit manifest schema version" "dry-run contract missing manifest schema metadata failure rule"
require_pattern "$RELEASE_DRY_RUN_DOC" "manifest, notes, or summary omit artifact digest algorithm or checksum sidecar format" "dry-run contract missing digest format metadata failure rule"
require_pattern "$RELEASE_DRY_RUN_DOC" "manifest, notes, or summary omit source mode" "dry-run contract missing source mode metadata failure rule"
require_pattern "$RELEASE_DRY_RUN_DOC" "manifest, notes, or summary omit release workflow run ID or URL" "dry-run contract missing workflow run metadata failure rule"
require_pattern "$RELEASE_DRY_RUN_DOC" "manifest, notes, or summary omit CI run ID, URL, or conclusion" "dry-run contract missing CI run metadata failure rule"
require_pattern "$RELEASE_DRY_RUN_DOC" "manifest, notes, or summary omit artifact count or platforms" "dry-run contract missing artifact metadata failure rule"
require_pattern "$RELEASE_DRY_RUN_DOC" "manifest, notes, or summary omit publication gate" "dry-run contract missing publication gate metadata failure rule"
require_pattern "$RELEASE_DRY_RUN_DOC" "manifest, notes, or summary omit adapter readiness status, gate, scope, or production boundary" "dry-run contract missing adapter readiness metadata failure rule"

require_pattern "$RELEASE_GATE_DOC" "release-workflow-publication-gate=same-commit-ci-release-metadata-and-github-release-publication-environment" "release gate missing publication gate marker"
require_pattern "$RELEASE_GATE_DOC" "release-workflow-publication-gate-evidence=publication-gate-in-manifest-notes-summary" "release gate missing publication gate evidence marker"
require_pattern "$RELEASE_GATE_DOC" "release-workflow-source-mode-evidence=source-mode-in-manifest-notes-summary" "release gate missing source mode evidence marker"
require_pattern "$RELEASE_GATE_DOC" "release-workflow-compatibility-summary=status-counts-in-manifest-notes-summary" "release gate missing compatibility summary marker"
require_pattern "$RELEASE_GATE_DOC" "release-workflow-manifest-schema=release-manifest-v1" "release gate missing manifest schema marker"
require_pattern "$RELEASE_GATE_DOC" "release-workflow-digest-format=sha256-sidecars" "release gate missing digest format marker"
require_pattern "$RELEASE_GATE_DOC" "release-workflow-run-evidence=run-id-url-in-manifest-notes-summary" "release gate missing workflow run evidence marker"
require_pattern "$RELEASE_GATE_DOC" "release-workflow-ci-run-evidence=ci-run-id-url-conclusion-in-manifest-notes-summary" "release gate missing CI run evidence marker"
require_pattern "$RELEASE_GATE_DOC" "release-workflow-artifact-evidence=artifact-count-and-platforms-in-manifest-notes-summary" "release gate missing artifact evidence marker"
require_pattern "$RELEASE_GATE_DOC" "release-workflow-adapter-readiness-manifest=ci-gated-alpha-boundary-fields" "release gate missing adapter readiness manifest marker"
require_pattern "$RELEASE_GATE_DOC" "release-workflow-compatibility-summary-static-check=scripts/compatibility-status-summary-check.sh" "release gate missing compatibility summary static check marker"
require_pattern "$RELEASE_GATE_DOC" "release-workflow-manual-intervention-static-check=scripts/manual-intervention-check.sh" "release gate missing manual intervention static check marker"
require_pattern "$RELEASE_GATE_DOC" "release-workflow-manual-intervention-transition-check=scripts/manual-intervention-transition-check.sh" "release gate missing manual intervention transition check marker"
require_pattern "$RELEASE_GATE_DOC" "release-workflow-release-checklist-static-check=scripts/release-checklist-check.sh" "release gate missing release checklist static check marker"
require_pattern "$RELEASE_GATE_DOC" "release-workflow-sensitive-material-gate=scripts/release-sensitive-material-check.sh" "release gate missing sensitive material gate marker"
require_pattern "$RELEASE_GATE_DOC" "release metadata omits compatibility status counts" "release gate missing metadata blocking condition"
require_pattern "$RELEASE_GATE_DOC" "release metadata omits manifest schema version" "release gate missing manifest schema metadata blocking condition"
require_pattern "$RELEASE_GATE_DOC" "release metadata omits artifact digest algorithm or checksum sidecar format" "release gate missing digest format metadata blocking condition"
require_pattern "$RELEASE_GATE_DOC" "release metadata omits source mode" "release gate missing source mode metadata blocking condition"
require_pattern "$RELEASE_GATE_DOC" "release metadata omits release workflow run ID or URL" "release gate missing workflow run metadata blocking condition"
require_pattern "$RELEASE_GATE_DOC" "release metadata omits CI run ID, URL, or conclusion" "release gate missing CI run metadata blocking condition"
require_pattern "$RELEASE_GATE_DOC" "release metadata omits artifact count or platforms" "release gate missing artifact metadata blocking condition"
require_pattern "$RELEASE_GATE_DOC" "release metadata omits publication gate" "release gate missing publication gate metadata blocking condition"
require_pattern "$RELEASE_GATE_DOC" "release metadata omits adapter readiness status, gate, scope, or production boundary" "release gate missing adapter readiness metadata blocking condition"
require_pattern "$RELEASE_GATE_DOC" "release notes omit compatibility scope, known gaps, or rollback path" "release gate missing notes blocking condition"

printf '%s\n' "release metadata check passed"
