#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
WORKSPACE=$(CDPATH= cd -- "$ROOT/.." && pwd)
MIHOMO_BIN=${MIHOMO_BIN:-"$WORKSPACE/downloads/mihomo-linux-amd64-compatible-v1.19.27"}
DEMO_DIR="$ROOT/examples/windows-bilibili"
DEMO_PLUGIN="$DEMO_DIR/bilibili-homepage.plugin"

if [ ! -x "$MIHOMO_BIN" ]; then
	echo "missing executable mihomo binary: $MIHOMO_BIN" >&2
	exit 1
fi
if [ ! -f "$DEMO_PLUGIN" ]; then
	echo "missing demo plugin: $DEMO_PLUGIN" >&2
	exit 1
fi

pick_port() {
	python3 - "$@" <<'PY'
import socket
sock = socket.socket()
sock.bind(("127.0.0.1", 0))
print(sock.getsockname()[1])
sock.close()
PY
}

wait_port() {
	host=$1
	port=$2
	name=$3
	for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
		if python3 - "$host" "$port" <<'PY'
import socket
import sys
sock = socket.socket()
sock.settimeout(0.25)
try:
    sock.connect((sys.argv[1], int(sys.argv[2])))
except OSError:
    sys.exit(1)
finally:
    sock.close()
PY
		then
			return 0
		fi
		sleep 0.2
	done
	echo "timeout waiting for $name on $host:$port" >&2
	return 1
}

assert_contains() {
	file=$1
	pattern=$2
	if ! grep -F "$pattern" "$file" >/dev/null; then
		echo "assertion failed: $file does not contain: $pattern" >&2
		echo "--- $file ---" >&2
		sed -n '1,180p' "$file" >&2
		exit 1
	fi
}

assert_header_contains_ci() {
	file=$1
	pattern=$2
	if ! grep -i -F "$pattern" "$file" >/dev/null; then
		echo "assertion failed: $file does not contain header: $pattern" >&2
		echo "--- $file ---" >&2
		sed -n '1,180p' "$file" >&2
		exit 1
	fi
}

assert_not_contains() {
	file=$1
	pattern=$2
	if grep -F "$pattern" "$file" >/dev/null; then
		echo "assertion failed: $file unexpectedly contains: $pattern" >&2
		echo "--- $file ---" >&2
		sed -n '1,180p' "$file" >&2
		exit 1
	fi
}

TMP=${TMPDIR:-/tmp}/anixops-bilibili-homepage-demo-$$
mkdir -p "$TMP/mihomo-home"
cleanup() {
	if [ "${SHIM_PID:-}" ]; then
		kill "$SHIM_PID" 2>/dev/null || true
	fi
	if [ "${MIHOMO_PID:-}" ]; then
		kill "$MIHOMO_PID" 2>/dev/null || true
	fi
	rm -rf "$TMP"
}
trap cleanup EXIT INT TERM

SHIM_PORT=$(pick_port)
MIHOMO_PORT=$(pick_port)
ORIGIN_PORT=$(pick_port)

cat > "$TMP/mihomo.yaml" <<EOF_CONF
mixed-port: ${MIHOMO_PORT}
bind-address: 127.0.0.1
allow-lan: false
mode: rule
log-level: warning
ipv6: false
hosts:
  www.bilibili.com: 127.0.0.1
rules:
  - MATCH,DIRECT
EOF_CONF

make -C "$ROOT" all >/dev/null
(cd "$ROOT/e2e/shim" && GO111MODULE=off go build -o "$ROOT/build/e2e_mitm_proxy_shim" .)

"$MIHOMO_BIN" -d "$TMP/mihomo-home" -f "$TMP/mihomo.yaml" >"$TMP/mihomo.log" 2>&1 &
MIHOMO_PID=$!
wait_port 127.0.0.1 "$MIHOMO_PORT" mihomo || {
	sed -n '1,200p' "$TMP/mihomo.log" >&2
	exit 1
}

"$ROOT/build/e2e_mitm_proxy_shim" \
	--listen "127.0.0.1:${SHIM_PORT}" \
	--origin-listen "127.0.0.1:${ORIGIN_PORT}" \
	--upstream "http://127.0.0.1:${MIHOMO_PORT}" \
	--config "$DEMO_PLUGIN" \
	--ca-cert "$TMP/ca.pem" \
	--ca-key "$TMP/ca.key" >"$TMP/shim.log" 2>&1 &
SHIM_PID=$!

wait_port 127.0.0.1 "$SHIM_PORT" shim || {
	sed -n '1,200p' "$TMP/shim.log" >&2
	exit 1
}

for _ in 1 2 3 4 5 6 7 8 9 10; do
	if [ -s "$TMP/ca.pem" ]; then
		break
	fi
	sleep 0.1
done
if [ ! -s "$TMP/ca.pem" ]; then
	echo "shim did not write CA certificate" >&2
	sed -n '1,200p' "$TMP/shim.log" >&2
	exit 1
fi

curl --silent --show-error --max-time 12 --http1.1 \
	--proxy "http://127.0.0.1:${SHIM_PORT}" \
	--cacert "$TMP/ca.pem" \
	--dump-header "$TMP/headers.out" \
	--output "$TMP/body.out" \
	"https://www.bilibili.com:${ORIGIN_PORT}/"

assert_contains "$TMP/headers.out" "HTTP/1.1 200 OK"
assert_header_contains_ci "$TMP/headers.out" "X-AnixOps-Bilibili-Demo: applied"
assert_header_contains_ci "$TMP/headers.out" "X-Origin-Accept-Encoding: identity"
assert_contains "$TMP/body.out" "anixops-bilibili-homepage-demo"
assert_contains "$TMP/body.out" "document.title = \"test\""
assert_contains "$TMP/body.out" "brightness(0)"
assert_contains "$TMP/body.out" "retryDelays"
assert_not_contains "$TMP/body.out" "MutationObserver"
assert_not_contains "$TMP/body.out" "setInterval"

curl --silent --show-error --max-time 12 --http1.1 \
	--proxy "http://127.0.0.1:${SHIM_PORT}" \
	--cacert "$TMP/ca.pem" \
	--dump-header "$TMP/asset-headers.out" \
	--output "$TMP/asset-body.out" \
	"https://www.bilibili.com:${ORIGIN_PORT}/gentleman/polyfill.js?features=es2015"

assert_contains "$TMP/asset-headers.out" "HTTP/1.1 200 OK"
assert_not_contains "$TMP/asset-headers.out" "X-AnixOps-Bilibili-Demo"
assert_contains "$TMP/asset-body.out" "console.log('polyfill');"
assert_not_contains "$TMP/asset-body.out" "anixops-bilibili-homepage-demo"

echo "bilibili homepage demo: HTML response script injected through shim -> mihomo path"
echo "mitm_anixops bilibili homepage demo e2e passed"
