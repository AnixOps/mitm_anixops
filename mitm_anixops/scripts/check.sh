#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

sh scripts/manual-intervention-check.sh
sh scripts/security-claim-check.sh
sh scripts/compatibility-evidence-check.sh
sh scripts/release-metadata-check.sh
sh scripts/v1-acceptance-check.sh
sh scripts/alpha-fixture-package-check.sh
make clean
make test
if [ -f /usr/include/pcre2.h ]; then
	make pcre2-test
else
	printf '%s\n' 'skip pcre2-test: missing /usr/include/pcre2.h'
fi
if [ -f /usr/include/jq.h ]; then
	make jq-test
else
	printf '%s\n' 'skip jq-test: missing /usr/include/jq.h'
fi
make all
make demo-check
make runner-check
make proxy-shim-check
if command -v pkg-config >/dev/null 2>&1; then
	make pkg-config-check
else
	printf '%s\n' 'skip pkg-config-check: pkg-config not found'
fi
if command -v go >/dev/null 2>&1; then
	make go-binding-check
else
	printf '%s\n' 'skip go-binding-check: go not found'
fi
if command -v cargo >/dev/null 2>&1; then
	make rust-binding-check
else
	printf '%s\n' 'skip rust-binding-check: cargo not found'
fi
make cmake-package-check
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
