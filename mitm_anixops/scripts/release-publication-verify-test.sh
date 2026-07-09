#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

fail() {
	printf '%s\n' "$1" >&2
	exit 1
}

require_file() {
	if [ ! -s "$1" ]; then
		fail "missing or empty fixture file: $1"
	fi
}

write_sidecar() {
	file=$1
	sidecar=$2
	name=$(basename "$file")

	sha256sum "$file" | awk -v name="$name" '{print $1 "  " name}' > "$sidecar"
}

copy_fixture_assets() {
	src=$1
	dst=$2

	mkdir -p "$dst"
	cp "$src"/* "$dst"/
}

make_fixture_assets() {
	assets=$1

	mkdir -p "$assets"

	linux_name="anixops-mitm-release-9.8.7.tar.gz"
	linux_sha_name="$linux_name.sha256"
	windows_name="anixops-mitm-release-windows-x64-9.8.7.zip"
	windows_sha_name="$windows_name.sha256"
	manifest_name="anixops-mitm-release-9.8.7-manifest.json"
	manifest_sha_name="$manifest_name.sha256"
	notes_name="release-notes.md"

	printf '%s\n' "linux fixture artifact" > "$assets/$linux_name"
	printf '%s\n' "windows fixture artifact" > "$assets/$windows_name"
	write_sidecar "$assets/$linux_name" "$assets/$linux_sha_name"
	write_sidecar "$assets/$windows_name" "$assets/$windows_sha_name"

	linux_hash=$(awk 'NR == 1 {print $1}' "$assets/$linux_sha_name")
	windows_hash=$(awk 'NR == 1 {print $1}' "$assets/$windows_sha_name")

	cat > "$assets/$manifest_name" <<EOF
{
  "release_version": "v9.8.7",
  "manifest_schema_version": "release-manifest-v1",
  "artifact_digest_algorithm": "sha256",
  "checksum_sidecar_format": "sha256sum-two-space-filename",
  "release_commit": "0123456789abcdef0123456789abcdef01234567",
  "source_mode": "tag",
  "release_workflow_run_id": "123456789",
  "release_workflow_run_url": "https://github.com/AnixOps/mitm_anixops/actions/runs/123456789",
  "ci_run_id": "987654321",
  "ci_run_url": "https://github.com/AnixOps/mitm_anixops/actions/runs/987654321",
  "ci_run_conclusion": "success",
  "publication_gate": "same-commit-ci-release-metadata-and-github-release-publication-environment",
  "artifact_count": 2,
  "artifact_platforms": ["linux-x64", "windows-x64"],
  "artifacts": [
    {
      "name": "$linux_name",
      "platform": "linux-x64",
      "sha256": "$linux_hash",
      "sha256_file": "$linux_sha_name"
    },
    {
      "name": "$windows_name",
      "platform": "windows-x64",
      "sha256": "$windows_hash",
      "sha256_file": "$windows_sha_name"
    }
  ],
  "adapter_readiness_status": "ci-gated-alpha-boundary",
  "adapter_readiness_gate": "scripts/integration-adapter-readiness-check.sh",
  "adapter_readiness_scope": "policy-core-runner-proxy-shim-bindings-docs",
  "adapter_readiness_production_claim": "not-production-networkcore-adapter",
  "compatibility_total_count": 35
}
EOF
	write_sidecar "$assets/$manifest_name" "$assets/$manifest_sha_name"

	cat > "$assets/$notes_name" <<'EOF'
# mitm_anixops v9.8.7

Source mode: tag
CI run: https://github.com/AnixOps/mitm_anixops/actions/runs/987654321
Release workflow run: https://github.com/AnixOps/mitm_anixops/actions/runs/123456789
Publication gate: same-commit-ci-release-metadata-and-github-release-publication-environment
Artifact count: 2; platforms: linux-x64,windows-x64.
Compatibility matrix status counts:
Adapter readiness: ci-gated-alpha-boundary
Rollback path: follow release rollback policy.
EOF

	for asset in \
		"$assets/$linux_name" \
		"$assets/$linux_sha_name" \
		"$assets/$windows_name" \
		"$assets/$windows_sha_name" \
		"$assets/$manifest_name" \
		"$assets/$manifest_sha_name" \
		"$assets/$notes_name"
	do
		require_file "$asset"
	done
}

run_verifier() {
	fixture_assets=$1

	GH_REPO=AnixOps/mitm_anixops \
	RELEASE_PUBLICATION_VERIFY_FIXTURE_ASSETS="$fixture_assets" \
	RELEASE_PUBLICATION_VERIFY_FIXTURE_VERSION=v9.8.7 \
	RELEASE_PUBLICATION_VERIFY_FIXTURE_COMMIT=0123456789abcdef0123456789abcdef01234567 \
	PATH="$fakebin:$PATH" \
		sh "$ROOT/scripts/release-publication-verify.sh" \
			v9.8.7 \
			0123456789abcdef0123456789abcdef01234567 \
			123456789 \
			987654321
}

tmpdir=$(mktemp -d)
cleanup() {
	rm -rf "$tmpdir"
}
trap cleanup EXIT HUP INT TERM

good_assets="$tmpdir/good-assets"
bad_assets="$tmpdir/bad-assets"
fakebin="$tmpdir/bin"
mkdir -p "$fakebin"

cat > "$fakebin/gh" <<'EOF'
#!/bin/sh
set -eu

if [ "$1" != "release" ]; then
	printf 'unsupported gh command: %s\n' "$1" >&2
	exit 1
fi

case "$2" in
	view)
		version=$3
		if [ "$version" != "$RELEASE_PUBLICATION_VERIFY_FIXTURE_VERSION" ]; then
			printf 'unexpected release view version: %s\n' "$version" >&2
			exit 1
		fi
		printf '{"assets":[{"name":"a"},{"name":"b"},{"name":"c"},{"name":"d"},{"name":"e"},{"name":"f"},{"name":"g"}],"targetCommitish":"%s","url":"https://github.com/AnixOps/mitm_anixops/releases/tag/%s"}\n' \
			"$RELEASE_PUBLICATION_VERIFY_FIXTURE_COMMIT" \
			"$RELEASE_PUBLICATION_VERIFY_FIXTURE_VERSION"
		;;
	download)
		version=$3
		if [ "$version" != "$RELEASE_PUBLICATION_VERIFY_FIXTURE_VERSION" ]; then
			printf 'unexpected release download version: %s\n' "$version" >&2
			exit 1
		fi
		shift 3
		dest=
		while [ "$#" -gt 0 ]; do
			case "$1" in
				-D)
					dest=$2
					shift 2
					;;
				--repo)
					shift 2
					;;
				*)
					shift
					;;
			esac
		done
		if [ -z "$dest" ]; then
			printf '%s\n' 'missing release download destination' >&2
			exit 1
		fi
		cp "$RELEASE_PUBLICATION_VERIFY_FIXTURE_ASSETS"/* "$dest"/
		;;
	*)
		printf 'unsupported gh release command: %s\n' "$2" >&2
		exit 1
		;;
esac
EOF
chmod +x "$fakebin/gh"

make_fixture_assets "$good_assets"
copy_fixture_assets "$good_assets" "$bad_assets"

positive_output="$tmpdir/positive.out"
run_verifier "$good_assets" > "$positive_output"
grep -F 'release_publication_verify_status=passed' "$positive_output" >/dev/null
grep -F 'release_version=v9.8.7' "$positive_output" >/dev/null
grep -F 'artifact_platforms=linux-x64,windows-x64' "$positive_output" >/dev/null

bad_hash=0000000000000000000000000000000000000000000000000000000000000000
printf '%s  %s\n' "$bad_hash" "anixops-mitm-release-9.8.7.tar.gz" > "$bad_assets/anixops-mitm-release-9.8.7.tar.gz.sha256"
negative_output="$tmpdir/negative.out"
negative_error="$tmpdir/negative.err"
if run_verifier "$bad_assets" > "$negative_output" 2> "$negative_error"; then
	fail "publication verifier accepted a mismatched checksum"
fi
grep -F 'sha256 mismatch for linux artifact' "$negative_error" >/dev/null

printf '%s\n' "release_publication_verify_test_status=passed"
