#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
MAKEFILE="$ROOT/Makefile"
FIXTURE_DIR="$ROOT/tests/fixtures"

test -f "$MAKEFILE"
test -d "$FIXTURE_DIR"

if ! grep -F 'cp tests/fixtures/*.* $(BUILD_DIR)/$(ALPHA_NAME)/fixtures/' "$MAKEFILE" >/dev/null; then
	printf '%s\n' "alpha fixture package check failed: alpha-dist must copy all top-level fixture files" >&2
	exit 1
fi

if ! grep -F 'cp tests/fixtures/corpus/manifest.json $(BUILD_DIR)/$(ALPHA_NAME)/fixtures/corpus/' "$MAKEFILE" >/dev/null; then
	printf '%s\n' "alpha fixture package check failed: corpus manifest copy is missing" >&2
	exit 1
fi

unmatched_fixture_count=$(
	find "$FIXTURE_DIR" -maxdepth 1 -type f ! -name '*.*' |
		wc -l |
		tr -d ' '
)
if [ "$unmatched_fixture_count" -ne 0 ]; then
	find "$FIXTURE_DIR" -maxdepth 1 -type f ! -name '*.*' >&2
	printf '%s\n' "alpha fixture package check failed: top-level fixture files must match tests/fixtures/*.*" >&2
	exit 1
fi

root_fixture_count=$(
	find "$FIXTURE_DIR" -maxdepth 1 -type f -name '*.*' |
		wc -l |
		tr -d ' '
)
if [ "$root_fixture_count" -eq 0 ]; then
	printf '%s\n' "alpha fixture package check failed: no top-level fixtures found" >&2
	exit 1
fi

printf 'alpha fixture package check passed (%s root fixtures)\n' "$root_fixture_count"
