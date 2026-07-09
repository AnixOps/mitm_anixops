#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

NAME_PATTERN='(^|/)(\.env($|[./])|id_(rsa|dsa|ecdsa|ed25519)$|[^/]*\.(pem|p12|pfx|key|jks|keystore|mobileprovision)$|[^/]*(secret|credentials?|token|private[-_]?key|passphrase)[^/]*$)'
CONTENT_PATTERN='-----BEGIN (RSA |DSA |EC |OPENSSH |ENCRYPTED )?PRIVATE KEY-----|AKIA[0-9A-Z]{16}|ghp_[0-9A-Za-z_]{36,}|github_pat_[0-9A-Za-z_]{20,}|xox[baprs]-[0-9A-Za-z-]{20,}'

if [ "$#" -eq 0 ]; then
	printf '%s\n' "release sensitive material check failed: no paths provided" >&2
	exit 1
fi

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

SCAN_COUNT=0

fail_sensitive() {
	printf '%s\n' "release sensitive material check failed: sensitive material detected: $1" >&2
	exit 1
}

check_member_name() {
	source=$1
	name=$2
	lower_name=$(printf '%s' "$name" | tr '[:upper:]' '[:lower:]')

	if printf '%s\n' "$lower_name" | grep -Eq -- "$NAME_PATTERN"; then
		fail_sensitive "$source:$name has a sensitive-looking path"
	fi
}

check_file_content() {
	source=$1
	file=$2

	if LC_ALL=C grep -Eiq -- "$CONTENT_PATTERN" "$file"; then
		fail_sensitive "$source matches a private key or token pattern"
	fi
}

scan_regular_file() {
	path=$1
	check_member_name "$path" "$(basename "$path")"
	check_file_content "$path" "$path"
	SCAN_COUNT=$((SCAN_COUNT + 1))
}

scan_directory() {
	path=$1
	list="$TMP_DIR/directory-files"
	find "$path" -type f | sort > "$list"
	while IFS= read -r file; do
		relative=${file#"$path"/}
		check_member_name "$path" "$relative"
		check_file_content "$path:$relative" "$file"
		SCAN_COUNT=$((SCAN_COUNT + 1))
	done < "$list"
}

scan_tar_gz() {
	path=$1
	list="$TMP_DIR/tar-files"
	content="$TMP_DIR/tar-content"

	if ! tar -tzf "$path" > "$list"; then
		printf '%s\n' "release sensitive material check failed: cannot list tar archive: $path" >&2
		exit 1
	fi

	while IFS= read -r member; do
		case "$member" in
			*/|"")
				continue
				;;
		esac
		check_member_name "$path" "$member"
		if tar -xOzf "$path" "$member" > "$content" 2>/dev/null; then
			check_file_content "$path:$member" "$content"
			SCAN_COUNT=$((SCAN_COUNT + 1))
		fi
	done < "$list"
}

scan_zip() {
	path=$1
	list="$TMP_DIR/zip-files"
	content="$TMP_DIR/zip-content"

	if ! command -v unzip >/dev/null 2>&1; then
		printf '%s\n' "release sensitive material check failed: unzip is required to scan zip artifacts" >&2
		exit 1
	fi
	if ! unzip -Z1 "$path" > "$list"; then
		printf '%s\n' "release sensitive material check failed: cannot list zip archive: $path" >&2
		exit 1
	fi

	while IFS= read -r member; do
		case "$member" in
			*/|"")
				continue
				;;
		esac
		check_member_name "$path" "$member"
		if unzip -p "$path" "$member" > "$content" 2>/dev/null; then
			check_file_content "$path:$member" "$content"
			SCAN_COUNT=$((SCAN_COUNT + 1))
		fi
	done < "$list"
}

for path in "$@"; do
	case "$path" in
		"$ROOT")
			printf '%s\n' "release sensitive material check failed: refusing to scan repository root as a release artifact" >&2
			exit 1
			;;
	esac
	if [ -d "$path" ]; then
		scan_directory "$path"
	elif [ -f "$path" ]; then
		case "$path" in
			*.tar.gz|*.tgz)
				scan_tar_gz "$path"
				;;
			*.zip)
				scan_zip "$path"
				;;
			*)
				scan_regular_file "$path"
				;;
		esac
	else
		printf '%s\n' "release sensitive material check failed: path not found: $path" >&2
		exit 1
	fi
done

printf 'release sensitive material check passed (%s files scanned)\n' "$SCAN_COUNT"
