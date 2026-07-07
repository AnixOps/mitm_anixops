#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
WORKSPACE=$(CDPATH= cd -- "$ROOT/.." && pwd)

MIHOMO_VERSION=${MIHOMO_VERSION:-v1.19.27}
MIHOMO_NAME=${MIHOMO_NAME:-mihomo-linux-amd64-compatible-v1.19.27}
MIHOMO_URL=${MIHOMO_URL:-"https://github.com/MetaCubeX/mihomo/releases/download/${MIHOMO_VERSION}/${MIHOMO_NAME}.gz"}
MIHOMO_GZ_SHA256=${MIHOMO_GZ_SHA256:-36850c946615f5c712946b62dbbbd06f6941d6d8a7543b315198bcb24ada3ea9}
MIHOMO_SHA256=${MIHOMO_SHA256:-ab33dead65aaa95bc03b722f155d95f1003ac00517de6726e9b8d56ef9bbec92}
MIHOMO_BIN=${MIHOMO_BIN:-"$WORKSPACE/downloads/$MIHOMO_NAME"}
MIHOMO_GZ=${MIHOMO_GZ:-"$MIHOMO_BIN.gz"}

sha256_file() {
	sha256sum "$1" | awk '{print $1}'
}

verify_file() {
	path=$1
	expected=$2
	label=$3
	actual=$(sha256_file "$path")
	if [ "$actual" != "$expected" ]; then
		echo "unexpected $label sha256: $actual" >&2
		echo "expected: $expected" >&2
		exit 1
	fi
}

mkdir -p "$(dirname "$MIHOMO_BIN")"

if [ -x "$MIHOMO_BIN" ]; then
	verify_file "$MIHOMO_BIN" "$MIHOMO_SHA256" "$MIHOMO_NAME"
	echo "mihomo fixture ready: $MIHOMO_BIN"
	exit 0
fi

if [ ! -f "$MIHOMO_GZ" ]; then
	curl -fL -o "$MIHOMO_GZ" "$MIHOMO_URL"
fi

verify_file "$MIHOMO_GZ" "$MIHOMO_GZ_SHA256" "$MIHOMO_NAME.gz"
gzip -dc "$MIHOMO_GZ" > "$MIHOMO_BIN"
chmod +x "$MIHOMO_BIN"
verify_file "$MIHOMO_BIN" "$MIHOMO_SHA256" "$MIHOMO_NAME"

echo "mihomo fixture ready: $MIHOMO_BIN"
