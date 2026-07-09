#!/bin/sh
set -eu

usage() {
	cat >&2 <<'USAGE'
usage: sh scripts/release-publication-verify.sh VERSION EXPECTED_COMMIT EXPECTED_RELEASE_RUN_ID EXPECTED_CI_RUN_ID

Verifies a published GitHub Release by downloading its assets and checking the
release manifest, release notes, checksum sidecars, target commit, release run,
same-commit CI run, artifact count, and artifact platforms.
USAGE
}

fail() {
	printf '%s\n' "$1" >&2
	exit 1
}

require_command() {
	if ! command -v "$1" >/dev/null 2>&1; then
		fail "missing required command: $1"
	fi
}

require_file() {
	if [ ! -s "$1" ]; then
		fail "missing or empty file: $1"
	fi
}

require_value() {
	label=$1
	value=$2
	if [ -z "$value" ] || [ "$value" = "null" ]; then
		fail "missing value: $label"
	fi
}

verify_sha_sidecar() {
	file=$1
	sidecar=$2
	label=$3

	require_file "$file"
	require_file "$sidecar"

	expected_hash=$(awk 'NR == 1 {print $1}' "$sidecar")
	expected_name=$(awk 'NR == 1 {print $2}' "$sidecar")
	actual_name=$(basename "$file")
	require_value "$label checksum" "$expected_hash"
	require_value "$label sidecar filename" "$expected_name"

	case "$expected_hash" in
		*[!0-9a-f]*)
			fail "invalid $label sha256 sidecar hash: $expected_hash"
			;;
	esac
	if [ "${#expected_hash}" -ne 64 ]; then
		fail "invalid $label sha256 sidecar hash length: $expected_hash"
	fi
	if [ "$expected_name" != "$actual_name" ]; then
		fail "invalid $label sha256 sidecar filename: $expected_name != $actual_name"
	fi

	actual_hash=$(sha256sum "$file" | awk '{print $1}')
	if [ "$actual_hash" != "$expected_hash" ]; then
		fail "sha256 mismatch for $label: $actual_hash != $expected_hash"
	fi
}

if [ "$#" -ne 4 ]; then
	usage
	exit 2
fi

for command_name in gh jq sha256sum awk grep mktemp find wc sort basename; do
	require_command "$command_name"
done

version=$1
expected_commit=$2
expected_release_run_id=$3
expected_ci_run_id=$4
gh_repo=${GH_REPO:-AnixOps/mitm_anixops}

case "$version" in
	v[0-9]*.[0-9]*.[0-9]*)
		;;
	*)
		fail "release version must look like vMAJOR.MINOR.PATCH: $version"
		;;
esac

version_no_v=${version#v}
manifest_name="anixops-mitm-release-${version_no_v}-manifest.json"
notes_name="release-notes.md"

tmpdir=$(mktemp -d)
cleanup() {
	rm -rf "$tmpdir"
}
trap cleanup EXIT HUP INT TERM

release_json=$(gh release view "$version" --repo "$gh_repo" --json assets,targetCommitish,url)
asset_count=$(printf '%s\n' "$release_json" | jq -r '.assets | length')
target_commit=$(printf '%s\n' "$release_json" | jq -r '.targetCommitish')
release_url=$(printf '%s\n' "$release_json" | jq -r '.url')

if [ "$asset_count" != "7" ]; then
	fail "unexpected GitHub Release asset count for $version: $asset_count"
fi
if [ "$target_commit" != "$expected_commit" ]; then
	fail "unexpected release target commit for $version: $target_commit != $expected_commit"
fi

gh release download "$version" --repo "$gh_repo" -D "$tmpdir" >/dev/null

downloaded_asset_count=$(find "$tmpdir" -maxdepth 1 -type f | wc -l | awk '{print $1}')
if [ "$downloaded_asset_count" != "7" ]; then
	fail "unexpected downloaded asset count for $version: $downloaded_asset_count"
fi

manifest="$tmpdir/$manifest_name"
manifest_sidecar="$tmpdir/$manifest_name.sha256"
notes="$tmpdir/$notes_name"

require_file "$manifest"
require_file "$manifest_sidecar"
require_file "$notes"

jq -e \
	--arg version "$version" \
	--arg commit "$expected_commit" \
	--arg release_run "$expected_release_run_id" \
	--arg ci_run "$expected_ci_run_id" '
	.release_version == $version and
	.manifest_schema_version == "release-manifest-v1" and
	.artifact_digest_algorithm == "sha256" and
	.checksum_sidecar_format == "sha256sum-two-space-filename" and
	.release_commit == $commit and
	.source_mode == "tag" and
	.release_workflow_run_id == $release_run and
	.release_workflow_run_url == ("https://github.com/AnixOps/mitm_anixops/actions/runs/" + $release_run) and
	.ci_run_id == $ci_run and
	.ci_run_url == ("https://github.com/AnixOps/mitm_anixops/actions/runs/" + $ci_run) and
	.ci_run_conclusion == "success" and
	.publication_gate == "same-commit-ci-release-metadata-and-github-release-publication-environment" and
	.artifact_count == 2 and
	(.artifact_platforms | sort) == ["linux-x64", "windows-x64"] and
	(.artifacts | length) == 2 and
	([.artifacts[].platform] | sort) == ["linux-x64", "windows-x64"] and
	.adapter_readiness_status == "ci-gated-alpha-boundary" and
	.adapter_readiness_gate == "scripts/integration-adapter-readiness-check.sh" and
	.adapter_readiness_scope == "policy-core-runner-proxy-shim-bindings-docs" and
	.adapter_readiness_production_claim == "not-production-networkcore-adapter" and
	.compatibility_total_count == 35
	' "$manifest" >/dev/null

linux_artifact_name=$(jq -r '.artifacts[] | select(.platform == "linux-x64") | .name' "$manifest")
linux_artifact_sidecar=$(jq -r '.artifacts[] | select(.platform == "linux-x64") | .sha256_file' "$manifest")
windows_artifact_name=$(jq -r '.artifacts[] | select(.platform == "windows-x64") | .name' "$manifest")
windows_artifact_sidecar=$(jq -r '.artifacts[] | select(.platform == "windows-x64") | .sha256_file' "$manifest")

require_value "linux artifact name" "$linux_artifact_name"
require_value "linux artifact sidecar" "$linux_artifact_sidecar"
require_value "windows artifact name" "$windows_artifact_name"
require_value "windows artifact sidecar" "$windows_artifact_sidecar"

verify_sha_sidecar "$tmpdir/$linux_artifact_name" "$tmpdir/$linux_artifact_sidecar" "linux artifact"
verify_sha_sidecar "$tmpdir/$windows_artifact_name" "$tmpdir/$windows_artifact_sidecar" "windows artifact"
verify_sha_sidecar "$manifest" "$manifest_sidecar" "release manifest"

grep -F 'Source mode: tag' "$notes" >/dev/null
grep -F 'CI run:' "$notes" >/dev/null
grep -F 'Release workflow run:' "$notes" >/dev/null
grep -F 'Publication gate:' "$notes" >/dev/null
grep -F 'Artifact count: 2; platforms: linux-x64,windows-x64.' "$notes" >/dev/null
grep -F 'Compatibility matrix status counts:' "$notes" >/dev/null
grep -F 'Adapter readiness:' "$notes" >/dev/null
grep -F 'Rollback path:' "$notes" >/dev/null

printf 'release_publication_verify_status=passed\n'
printf 'release_version=%s\n' "$version"
printf 'release_url=%s\n' "$release_url"
printf 'asset_count=%s\n' "$asset_count"
printf 'target_commit=%s\n' "$target_commit"
printf 'release_workflow_run_id=%s\n' "$expected_release_run_id"
printf 'ci_run_id=%s\n' "$expected_ci_run_id"
printf 'artifact_count=2\n'
printf 'artifact_platforms=linux-x64,windows-x64\n'
