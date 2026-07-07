#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
WORKSPACE=$(CDPATH= cd -- "$ROOT/.." && pwd)
MIHOMO_BIN=${MIHOMO_BIN:-"$WORKSPACE/downloads/mihomo-linux-amd64-compatible-v1.19.27"}
BILI_PLUGIN=${BILI_PLUGIN:-"$ROOT/tests/fixtures/BiliBili.Enhanced.plugin"}
BILI_SCRIPT=${BILI_SCRIPT:-"$WORKSPACE/downloads/BiliBili.Enhanced.response.bundle.js"}
BILI_SCRIPT_URL="https://github.com/BiliUniverse/Enhanced/releases/download/v0.5.13/response.bundle.js"
BILI_SCRIPT_SHA256="f34238dce8fc4c525a371e934382d2241a6712a366de609892c710712808f445"

if [ ! -x "$MIHOMO_BIN" ]; then
	echo "missing executable mihomo binary: $MIHOMO_BIN" >&2
	echo "set MIHOMO_BIN=/path/to/mihomo or download the official MetaCubeX/mihomo release asset" >&2
	exit 1
fi

if [ ! -f "$BILI_PLUGIN" ]; then
	echo "missing Bili plugin fixture: $BILI_PLUGIN" >&2
	exit 1
fi

if [ ! -f "$BILI_SCRIPT" ]; then
	mkdir -p "$(dirname "$BILI_SCRIPT")"
	curl -L -o "$BILI_SCRIPT" "$BILI_SCRIPT_URL"
fi
actual_bili_sha=$(sha256sum "$BILI_SCRIPT" | awk '{print $1}')
if [ "$actual_bili_sha" != "$BILI_SCRIPT_SHA256" ]; then
	echo "unexpected response.bundle.js sha256: $actual_bili_sha" >&2
	echo "expected: $BILI_SCRIPT_SHA256" >&2
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
		sed -n '1,160p' "$file" >&2
		exit 1
	fi
}

assert_header_contains_ci() {
	file=$1
	pattern=$2
	if ! grep -i -F "$pattern" "$file" >/dev/null; then
		echo "assertion failed: $file does not contain header: $pattern" >&2
		echo "--- $file ---" >&2
		sed -n '1,160p' "$file" >&2
		exit 1
	fi
}

TMP=${TMPDIR:-/tmp}/anixops-e2e-$$
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

cat > "$TMP/anixops-e2e.config" <<EOF_CONF
[Rewrite]
^https://google\\.com:${ORIGIN_PORT}/e2e-redirect$ https://google.com:${ORIGIN_PORT}/e2e-target 302

[MITM]
enable = true
hostname = google.com
EOF_CONF

cat "$BILI_PLUGIN" >> "$TMP/anixops-e2e.config"

cat > "$TMP/mihomo.yaml" <<EOF_CONF
mixed-port: ${MIHOMO_PORT}
bind-address: 127.0.0.1
allow-lan: false
mode: rule
log-level: warning
ipv6: false
hosts:
  google.com: 127.0.0.1
  app.bilibili.com: 127.0.0.1
  app.biliapi.net: 127.0.0.1
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
	--mihomo-proxy "http://127.0.0.1:${MIHOMO_PORT}" \
	--config "$TMP/anixops-e2e.config" \
	--script-runner "$ROOT/e2e/script_runtime/anixops_runner.js" \
	--script-path-url "$BILI_SCRIPT_URL" \
	--script-path-file "$BILI_SCRIPT" \
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

curl --silent --show-error --max-time 8 --http1.1 \
	--proxy "http://127.0.0.1:${SHIM_PORT}" \
	--cacert "$TMP/ca.pem" \
	--dump-header "$TMP/redirect.headers" \
	--output "$TMP/redirect.body" \
	"https://google.com:${ORIGIN_PORT}/e2e-redirect"
assert_contains "$TMP/redirect.headers" "HTTP/1.1 302 Found"
assert_header_contains_ci "$TMP/redirect.headers" "Location: https://google.com:${ORIGIN_PORT}/e2e-target"

curl --silent --show-error --max-time 8 --http1.1 \
	--proxy "http://127.0.0.1:${SHIM_PORT}" \
	--cacert "$TMP/ca.pem" \
	--output "$TMP/pass.body" \
	"https://google.com:${ORIGIN_PORT}/e2e-pass"
assert_contains "$TMP/pass.body" "origin-ok host=google.com:${ORIGIN_PORT} path=/e2e-pass"

curl --silent --show-error --max-time 12 --http1.1 \
	--proxy "http://127.0.0.1:${SHIM_PORT}" \
	--cacert "$TMP/ca.pem" \
	--output "$TMP/bili-tab.body" \
	"https://app.bilibili.com:${ORIGIN_PORT}/x/resource/show/tab/v2?build=1"
node - "$TMP/bili-tab.body" <<'JS'
const fs = require("node:fs");
const body = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
if (!body.data || !Array.isArray(body.data.tab) || body.data.tab.length !== 7) {
  throw new Error(`unexpected Bili tab count: ${body.data && body.data.tab && body.data.tab.length}`);
}
if (!Array.isArray(body.data.top) || body.data.top.length !== 1) {
  throw new Error(`unexpected Bili top count: ${body.data && body.data.top && body.data.top.length}`);
}
if (!body.data.top_left || body.data.top_left.url !== "bilibili://user_center/") {
  throw new Error("Bili top_left was not rewritten");
}
JS

echo "mihomo binary: $MIHOMO_BIN"
"$MIHOMO_BIN" -v
echo "e2e redirect: https://google.com:${ORIGIN_PORT}/e2e-redirect -> 302 Location"
echo "e2e pass-through: client -> shim MITM -> mihomo -> local HTTPS origin"
echo "e2e script: BiliBili.Enhanced plugin response hook executed through MITM proxy path"
echo "mitm_anixops mihomo e2e passed"
