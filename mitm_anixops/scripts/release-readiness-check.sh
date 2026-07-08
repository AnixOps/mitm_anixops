#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=${1:-}
MANUAL="$ROOT/docs/manual-intervention.md"
MATRIX="$ROOT/docs/compatibility/matrix.md"

test -f "$MANUAL"
test -f "$MATRIX"

if [ -z "$VERSION" ]; then
	printf '%s\n' "release readiness version is required" >&2
	exit 1
fi

if ! printf '%s\n' "$VERSION" | grep -Eq '^v[0-9]+\.[0-9]+\.[0-9]+(-(alpha|beta|rc)(\.[0-9]+)?)?$'; then
	printf 'invalid release readiness version: %s\n' "$VERSION" >&2
	exit 1
fi

case "$VERSION" in
	*-alpha* | *-beta* | *-rc*)
		printf 'release_readiness_status=not-required-prerelease\n'
		printf 'release_readiness_blocking_reason=prerelease-does-not-claim-v1-stable-readiness\n'
		printf 'release_readiness_blocking_markers=none\n'
		printf 'release_readiness_planned_row_count=not-checked-for-prerelease\n'
		exit 0
		;;
esac

blocking_markers=""
for item in $(awk -F= '
$1 ~ /-required-before$/ && $2 ~ /^v1\.0\.0(-release)?$/ {
	name = $1
	sub(/-required-before$/, "", name)
	print name
}
' "$MANUAL"); do
	if grep -F "${item}-status=pending" "$MANUAL" >/dev/null; then
		if [ -n "$blocking_markers" ]; then
			blocking_markers="${blocking_markers},${item}"
		else
			blocking_markers="$item"
		fi
	fi
done

planned_row_count=$(grep -Ec '^\| [^|]+ \| [^|]+ \| planned \|' "$MATRIX" || true)

if [ -n "$blocking_markers" ] || [ "$planned_row_count" -ne 0 ]; then
	if [ -n "$blocking_markers" ] && [ "$planned_row_count" -ne 0 ]; then
		blocking_reason="manual-intervention-required-before-v1-and-planned-matrix-rows"
	elif [ -n "$blocking_markers" ]; then
		blocking_reason="manual-intervention-required-before-v1"
	else
		blocking_reason="planned-compatibility-matrix-rows"
	fi
	printf 'release_readiness_status=blocked\n'
	printf 'release_readiness_blocking_reason=%s\n' "$blocking_reason"
	printf 'release_readiness_blocking_markers=%s\n' "${blocking_markers:-none}"
	printf 'release_readiness_planned_row_count=%s\n' "$planned_row_count"
	exit 1
fi

printf 'release_readiness_status=passed\n'
printf 'release_readiness_blocking_reason=none\n'
printf 'release_readiness_blocking_markers=none\n'
printf 'release_readiness_planned_row_count=0\n'
