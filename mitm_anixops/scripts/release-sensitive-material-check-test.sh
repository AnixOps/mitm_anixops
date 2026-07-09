#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CHECK="$ROOT/scripts/release-sensitive-material-check.sh"

test -s "$CHECK"
command -v tar >/dev/null 2>&1
command -v zip >/dev/null 2>&1
command -v unzip >/dev/null 2>&1

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

clean_dir="$TMP_DIR/clean-package"
mkdir -p "$clean_dir/docs" "$clean_dir/bin"
printf '%s\n' "clean release package fixture" > "$clean_dir/README.md"
printf '%s\n' "metadata only; no private key or token marker" > "$clean_dir/docs/scope.txt"
printf '%s\n' "#!/bin/sh" "printf '%s\\n' ok" > "$clean_dir/bin/demo"

clean_tar="$TMP_DIR/clean-package.tar.gz"
tar -C "$TMP_DIR" -czf "$clean_tar" clean-package

clean_zip="$TMP_DIR/clean-package.zip"
(cd "$TMP_DIR" && zip -qr "$clean_zip" clean-package)

sh "$CHECK" "$clean_dir" "$clean_tar" "$clean_zip" >/dev/null

blocked_dir="$TMP_DIR/blocked-package"
mkdir -p "$blocked_dir/keys" "$blocked_dir/docs"
printf '%s\n' "normal file" > "$blocked_dir/docs/readme.txt"
{
	printf '%s\n' "-----BEGIN PRIVATE KEY-----"
	printf '%s\n' "not-a-real-key-test-fixture"
	printf '%s\n' "-----END PRIVATE KEY-----"
} > "$blocked_dir/keys/service.key"

if sh "$CHECK" "$blocked_dir" > "$TMP_DIR/blocked-dir.out" 2> "$TMP_DIR/blocked-dir.err"; then
	printf '%s\n' "release sensitive material check test failed: blocked directory passed" >&2
	exit 1
fi
grep -q "sensitive material detected" "$TMP_DIR/blocked-dir.err"

blocked_tar="$TMP_DIR/blocked-package.tar.gz"
tar -C "$TMP_DIR" -czf "$blocked_tar" blocked-package
if sh "$CHECK" "$blocked_tar" > "$TMP_DIR/blocked-tar.out" 2> "$TMP_DIR/blocked-tar.err"; then
	printf '%s\n' "release sensitive material check test failed: blocked tarball passed" >&2
	exit 1
fi
grep -q "sensitive material detected" "$TMP_DIR/blocked-tar.err"

blocked_zip="$TMP_DIR/blocked-package.zip"
(cd "$TMP_DIR" && zip -qr "$blocked_zip" blocked-package)
if sh "$CHECK" "$blocked_zip" > "$TMP_DIR/blocked-zip.out" 2> "$TMP_DIR/blocked-zip.err"; then
	printf '%s\n' "release sensitive material check test failed: blocked zip passed" >&2
	exit 1
fi
grep -q "sensitive material detected" "$TMP_DIR/blocked-zip.err"

printf '%s\n' "release sensitive material check test passed"
