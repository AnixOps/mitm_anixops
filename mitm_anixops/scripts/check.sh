#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

make clean
make test
make all
make demo-check
make ordering-contract-check

DEFAULT_MIHOMO_BIN="$ROOT/../downloads/mihomo-linux-amd64-compatible-v1.19.27"
MIHOMO_BIN=${MIHOMO_BIN:-"$DEFAULT_MIHOMO_BIN"}

if command -v nm >/dev/null 2>&1; then
	nm -D --defined-only build/libmitm_anixops.so |
		awk '$3 ~ /^anixops_/ { print $3 }' |
		sort > build/abi_exports.actual
	diff -u ci/abi_exports.txt build/abi_exports.actual
else
	printf '%s\n' "skip ABI export allowlist: nm not found"
fi

if command -v readelf >/dev/null 2>&1; then
	readelf -h build/libmitm_anixops.so >/dev/null
fi

if [ -x "$MIHOMO_BIN" ]; then
	MIHOMO_BIN="$MIHOMO_BIN" make e2e
else
	printf '%s\n' "skip mihomo proxy e2e: missing executable $MIHOMO_BIN"
fi

make bili-e2e
make script-contract-e2e
make bilibili-homepage-demo-e2e

printf '%s\n' 'mitm_anixops check passed'
