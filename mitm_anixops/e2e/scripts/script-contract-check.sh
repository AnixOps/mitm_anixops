#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
WORKSPACE=$(CDPATH= cd -- "$ROOT/.." && pwd)
MIHOMO_BIN=${MIHOMO_BIN:-"$WORKSPACE/downloads/mihomo-linux-amd64-compatible-v1.19.27"}
SCRIPT_FILE=${SCRIPT_FILE:-"$ROOT/e2e/script_runtime/response_mutator.js"}
SCRIPT_URL="https://scripts.example.test/response_mutator.js"

if [ ! -x "$MIHOMO_BIN" ]; then
	echo "missing executable mihomo binary: $MIHOMO_BIN" >&2
	exit 1
fi
if [ ! -f "$SCRIPT_FILE" ]; then
	echo "missing script file: $SCRIPT_FILE" >&2
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

TMP=${TMPDIR:-/tmp}/anixops-script-contract-$$
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

cat > "$TMP/module.plugin" <<EOF_CONF
[Argument]
Mode = input,contract

[Script]
http-request ^https:\\/\\/google\\.com\\/contract\\/request-response\\? requires-body=1, script-path=${SCRIPT_URL}, tag=contract.request, argument=[{Mode}]
http-response ^https:\\/\\/google\\.com\\/contract\\/request-response\\? requires-body=1, script-path=${SCRIPT_URL}, tag=contract.response, argument=[{Mode}]

[MITM]
hostname = google.com
EOF_CONF

cat > "$TMP/mihomo.yaml" <<EOF_CONF
mixed-port: ${MIHOMO_PORT}
bind-address: 127.0.0.1
allow-lan: false
mode: rule
log-level: warning
ipv6: false
hosts:
  google.com: 127.0.0.1
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
	--config "$TMP/module.plugin" \
	--script-runner "$ROOT/e2e/script_runtime/anixops_runner.js" \
	--script-path-url "$SCRIPT_URL" \
	--script-path-file "$SCRIPT_FILE" \
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
	--request POST \
	--header "Content-Type: application/json" \
	--data '{"from":"curl"}' \
	--dump-header "$TMP/headers.out" \
	--output "$TMP/body.out" \
	"https://google.com:${ORIGIN_PORT}/contract/request-response?contract=1"

grep -F "HTTP/1.1 201 Created" "$TMP/headers.out" >/dev/null
grep -i -F "X-AnixOps-Script: applied" "$TMP/headers.out" >/dev/null

node - "$TMP/body.out" <<'JS'
const fs = require("node:fs");
const body = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
if (body.ok !== true) throw new Error("script body was not applied");
if (body.argument !== "Mode=contract") throw new Error(`unexpected argument: ${body.argument}`);
if (body.url !== "https://google.com/contract/request-response?contract=1") throw new Error(`unexpected URL: ${body.url}`);
if (body.requestHeader !== "applied") throw new Error(`request header did not reach response script: ${body.requestHeader}`);
if (!body.original || body.original.code !== 0) throw new Error("original response body missing");
if (body.original.requestScript !== "applied") throw new Error("origin did not receive request script header");
const upstream = JSON.parse(body.original.body);
if (upstream.from !== "curl") throw new Error("origin did not receive original request body");
if (upstream.requestScript !== true) throw new Error("origin did not receive request script body mutation");
if (upstream.argument !== "Mode=contract") throw new Error("request script did not receive resolved argument");
JS

echo "script contract: request header/body and response status/header/body applied through MITM proxy path"
echo "mitm_anixops script contract e2e passed"
