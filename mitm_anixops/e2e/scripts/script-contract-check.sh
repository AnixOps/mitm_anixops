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

[Rewrite]
^https://google\\.com:${ORIGIN_PORT}/contract/request-response\\? header-add X-AnixOps-Static-Request static-request
^https://google\\.com:${ORIGIN_PORT}/contract/request-response\\? request-body-json-replace $.from "static-request"
^https://google\\.com:${ORIGIN_PORT}/contract/request-response\\? response-header-add X-AnixOps-Static-Response static-response
^https://google\\.com:${ORIGIN_PORT}/contract/request-response\\? response-body-json-replace $.code 7
^https://google\\.com:${ORIGIN_PORT}/contract/request-response\\?request-compressed=gzip header-del Content-Encoding
^https://google\\.com:${ORIGIN_PORT}/contract/request-response\\?request-compressed=deflate header-replace Content-Encoding gzip
^https://google\\.com:${ORIGIN_PORT}/contract/request-response\\?compressed=gzip response-header-del Content-Encoding
^https://google\\.com:${ORIGIN_PORT}/contract/request-response\\?compressed=deflate response-header-replace Content-Encoding gzip

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
	--script-store "$TMP/script-store.json" \
	--script-timeout-ms 50 \
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
if (body.persistentArgument !== "Mode=contract") throw new Error(`response script did not read persistent store: ${body.persistentArgument}`);
if (body.requestHeader !== "applied") throw new Error(`request header did not reach response script: ${body.requestHeader}`);
if (body.staticResponseHeader !== "static-response") throw new Error(`response header rewrite did not run before response script: ${body.staticResponseHeader}`);
if (!body.original || body.original.code !== 7) throw new Error("response body rewrite did not run before response script");
if (body.original.requestScript !== "applied") throw new Error("origin did not receive request script header");
const upstream = JSON.parse(body.original.body);
if (upstream.staticRequestHeader !== "static-request") throw new Error(`request header rewrite did not run before request script: ${upstream.staticRequestHeader}`);
if (upstream.from !== "static-request") throw new Error("request body rewrite did not run before request script");
if (upstream.requestScript !== true) throw new Error("origin did not receive request script body mutation");
if (upstream.argument !== "Mode=contract") throw new Error("request script did not receive resolved argument");
if (upstream.storeAfter !== "Mode=contract") throw new Error(`request script did not write persistent store: ${upstream.storeAfter}`);
JS

curl --silent --show-error --max-time 12 --http1.1 \
	--proxy "http://127.0.0.1:${SHIM_PORT}" \
	--cacert "$TMP/ca.pem" \
	--request POST \
	--header "Content-Type: application/json" \
	--data '{"from":"timeout"}' \
	--dump-header "$TMP/timeout-headers.out" \
	--output "$TMP/timeout-body.out" \
	"https://google.com:${ORIGIN_PORT}/contract/request-response?timeout=1"

grep -F "HTTP/1.1 200 OK" "$TMP/timeout-headers.out" >/dev/null
grep -i -F "X-AnixOps-Static-Response: static-response" "$TMP/timeout-headers.out" >/dev/null
if grep -i -F "X-AnixOps-Script:" "$TMP/timeout-headers.out" >/dev/null; then
	echo "timed-out response script unexpectedly applied response headers" >&2
	sed -n '1,120p' "$TMP/timeout-headers.out" >&2
	exit 1
fi

node - "$TMP/timeout-body.out" <<'JS'
const fs = require("node:fs");
const body = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
if (body.code !== 7) throw new Error(`response body rewrite did not survive script timeout: ${body.code}`);
if (body.requestScript !== "applied") throw new Error("request script header did not survive response timeout");
const upstream = JSON.parse(body.body);
if (upstream.from !== "static-request") throw new Error("request body rewrite did not survive response timeout");
if (upstream.requestScript !== true) throw new Error("request script body mutation did not survive response timeout");
JS

curl --silent --show-error --max-time 12 --http1.1 \
	--proxy "http://127.0.0.1:${SHIM_PORT}" \
	--cacert "$TMP/ca.pem" \
	--request POST \
	--header "Content-Type: application/json" \
	--data '{"from":"throw"}' \
	--dump-header "$TMP/throw-headers.out" \
	--output "$TMP/throw-body.out" \
	"https://google.com:${ORIGIN_PORT}/contract/request-response?throw=1"

grep -F "HTTP/1.1 200 OK" "$TMP/throw-headers.out" >/dev/null
grep -i -F "X-AnixOps-Static-Response: static-response" "$TMP/throw-headers.out" >/dev/null
if grep -i -F "X-AnixOps-Script:" "$TMP/throw-headers.out" >/dev/null; then
	echo "failed response script unexpectedly applied response headers" >&2
	sed -n '1,120p' "$TMP/throw-headers.out" >&2
	exit 1
fi

node - "$TMP/throw-body.out" <<'JS'
const fs = require("node:fs");
const body = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
if (body.code !== 7) throw new Error(`response body rewrite did not survive script exception: ${body.code}`);
if (body.requestScript !== "applied") throw new Error("request script header did not survive response exception");
const upstream = JSON.parse(body.body);
if (upstream.from !== "static-request") throw new Error("request body rewrite did not survive response exception");
if (upstream.requestScript !== true) throw new Error("request script body mutation did not survive response exception");
JS

run_compressed_response_case() {
	encoding=$1
	curl --silent --show-error --max-time 12 --http1.1 \
		--proxy "http://127.0.0.1:${SHIM_PORT}" \
		--cacert "$TMP/ca.pem" \
		--request POST \
		--header "Content-Type: application/json" \
		--data "{\"from\":\"${encoding}\"}" \
		--dump-header "$TMP/${encoding}-headers.out" \
		--output "$TMP/${encoding}-body.out" \
		"https://google.com:${ORIGIN_PORT}/contract/request-response?compressed=${encoding}"

	grep -F "HTTP/1.1 201 Created" "$TMP/${encoding}-headers.out" >/dev/null
	grep -i -F "X-AnixOps-Script: applied" "$TMP/${encoding}-headers.out" >/dev/null
	if grep -i -F "Content-Encoding:" "$TMP/${encoding}-headers.out" >/dev/null; then
		echo "scripted ${encoding} response leaked Content-Encoding" >&2
		sed -n '1,120p' "$TMP/${encoding}-headers.out" >&2
		exit 1
	fi

	node - "$TMP/${encoding}-body.out" "$encoding" <<'JS'
const fs = require("node:fs");
const body = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
const encoding = process.argv[3];
if (body.ok !== true) throw new Error(`${encoding}: script body was not applied`);
if (body.url !== `https://google.com/contract/request-response?compressed=${encoding}`) {
  throw new Error(`${encoding}: unexpected URL: ${body.url}`);
}
if (body.persistentArgument !== "Mode=contract") {
  throw new Error(`${encoding}: response script did not read persistent store: ${body.persistentArgument}`);
}
if (!body.original || body.original.code !== 7) throw new Error(`${encoding}: response body rewrite did not run before response script`);
if (body.staticResponseHeader !== "static-response") {
  throw new Error(`${encoding}: response header rewrite did not run before response script: ${body.staticResponseHeader}`);
}
if (body.original.requestScript !== "applied") throw new Error(`${encoding}: request script header missing`);
const upstream = JSON.parse(body.original.body);
if (upstream.staticRequestHeader !== "static-request") {
  throw new Error(`${encoding}: request header rewrite did not run before request script: ${upstream.staticRequestHeader}`);
}
if (upstream.from !== "static-request") throw new Error(`${encoding}: request body rewrite did not run before request script`);
if (upstream.requestScript !== true) throw new Error(`${encoding}: request script body mutation missing`);
if (upstream.storeAfter !== "Mode=contract") throw new Error(`${encoding}: request script did not write persistent store`);
JS
}

run_compressed_request_case() {
	encoding=$1
	python3 - "$TMP/${encoding}-request.bin" "$encoding" <<'PY'
import gzip
import pathlib
import sys
import zlib

path = pathlib.Path(sys.argv[1])
encoding = sys.argv[2]
payload = ('{"from":"' + encoding + '"}').encode()
if encoding == "gzip":
    payload = gzip.compress(payload)
elif encoding == "deflate":
    payload = zlib.compress(payload)
else:
    raise SystemExit(f"unsupported test encoding: {encoding}")
path.write_bytes(payload)
PY

	curl --silent --show-error --max-time 12 --http1.1 \
		--proxy "http://127.0.0.1:${SHIM_PORT}" \
		--cacert "$TMP/ca.pem" \
		--request POST \
		--header "Content-Type: application/json" \
		--header "Content-Encoding: ${encoding}" \
		--data-binary "@${TMP}/${encoding}-request.bin" \
		--dump-header "$TMP/${encoding}-request-headers.out" \
		--output "$TMP/${encoding}-request-body.out" \
		"https://google.com:${ORIGIN_PORT}/contract/request-response?request-compressed=${encoding}"

	grep -F "HTTP/1.1 201 Created" "$TMP/${encoding}-request-headers.out" >/dev/null
	grep -i -F "X-AnixOps-Script: applied" "$TMP/${encoding}-request-headers.out" >/dev/null

	node - "$TMP/${encoding}-request-body.out" "$encoding" <<'JS'
const fs = require("node:fs");
const body = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
const encoding = process.argv[3];
if (body.ok !== true) throw new Error(`${encoding}: request script was not applied`);
if (!body.original) throw new Error(`${encoding}: missing origin response payload`);
const upstream = JSON.parse(body.original.body);
if (upstream.from !== "static-request") {
  throw new Error(`${encoding}: static body rewrite did not run after request decode`);
}
if (upstream.requestScript !== true) {
  throw new Error(`${encoding}: request script did not receive decoded JSON body`);
}
if (upstream.staticRequestHeader !== "static-request") {
  throw new Error(`${encoding}: static request header was not retained after body decode`);
}
if (body.original.requestEncoding !== "") {
  throw new Error(`${encoding}: decoded request leaked Content-Encoding: ${body.original.requestEncoding}`);
}
JS
}

run_compressed_request_overflow_case() {
	python3 - "$TMP/request-overflow.bin" <<'PY'
import gzip
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
path.write_bytes(gzip.compress(b"x" * (32 * 1024 * 1024 + 1)))
PY

	curl --silent --show-error --max-time 12 --http1.1 \
		--proxy "http://127.0.0.1:${SHIM_PORT}" \
		--cacert "$TMP/ca.pem" \
		--request POST \
		--header "Content-Type: application/json" \
		--header "Content-Encoding: gzip" \
		--data-binary "@${TMP}/request-overflow.bin" \
		--dump-header "$TMP/request-overflow-headers.out" \
		--output "$TMP/request-overflow-body.out" \
		"https://google.com:${ORIGIN_PORT}/contract/request-response?request-overflow=1&summary=1"

	grep -F "HTTP/1.1 201 Created" "$TMP/request-overflow-headers.out" >/dev/null
	grep -i -F "X-AnixOps-Script: applied" "$TMP/request-overflow-headers.out" >/dev/null

	node - "$TMP/request-overflow-body.out" <<'JS'
const fs = require("node:fs");
const body = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
if (body.ok !== true || !body.original) {
  throw new Error("overflow request did not reach the origin through the response contract");
}
if (body.original.requestEncoding !== "gzip") {
  throw new Error(`overflow request lost its raw Content-Encoding: ${body.original.requestEncoding}`);
}
if (!Number.isInteger(body.original.requestByteCount) || body.original.requestByteCount <= 0) {
  throw new Error(`overflow request did not retain a raw compressed body: ${body.original.requestByteCount}`);
}
JS
}

run_compressed_response_case gzip
run_compressed_response_case deflate
run_compressed_request_case gzip
run_compressed_request_case deflate
run_compressed_request_overflow_case

grep -F '"contract.argument": "Mode=contract"' "$TMP/script-store.json" >/dev/null

echo "script contract: request header/body and response status/header/body applied through MITM proxy path"
echo "script contract: static request/response header rewrites run before script dispatch"
echo "script contract: static request/response body rewrites run before script dispatch"
echo "script contract: persistentStore read/write shared across request and response scripts"
echo "script contract: response script timeout fails open after static rewrites"
echo "script contract: response script exception fails open after static rewrites"
echo "script contract: gzip/deflate response bodies decoded and returned as identity after script mutation"
echo "script contract: gzip/deflate request bodies decoded before body/script mutation and returned as identity"
echo "script contract: content-encoding header rewrites preserve a matching body representation"
echo "script contract: decoded request overflow relays the raw compressed body fail-open"
echo "mitm_anixops script contract e2e passed"
