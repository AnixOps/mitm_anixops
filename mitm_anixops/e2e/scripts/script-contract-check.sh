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
^https://google\\.com:${ORIGIN_PORT}/contract/binary-body request-body-replace-regex "__anixops_binary_no_match__" "replacement"
^https://google\\.com:${ORIGIN_PORT}/contract/binary-body response-body-replace-regex "__anixops_binary_no_match__" "replacement"
^https://google\\.com:${ORIGIN_PORT}/contract/binary-body request-body-replace-regex "^anixops$" "anixops"
^https://google\\.com:${ORIGIN_PORT}/contract/binary-body response-body-replace-regex "^anixops$" "anixops"

[Script]
http-request ^https:\\/\\/google\\.com\\/contract\\/request-response\\? requires-body=1, script-path=${SCRIPT_URL}, tag=contract.request, argument=[{Mode}]
http-response ^https:\\/\\/google\\.com\\/contract\\/request-response\\? requires-body=1, script-path=${SCRIPT_URL}, tag=contract.response, argument=[{Mode}]
http-request ^https:\\/\\/google\\.com\\/contract\\/binary-body requires-body=1, script-path=${SCRIPT_URL}, tag=contract.binary.request, argument=[{Mode}]
http-response ^https:\\/\\/google\\.com\\/contract\\/binary-body requires-body=1, script-path=${SCRIPT_URL}, tag=contract.binary.response, argument=[{Mode}]

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
	--debug \
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

curl --silent --show-error --max-time 12 --http1.1 \
	--proxy "http://127.0.0.1:${SHIM_PORT}" \
	--cacert "$TMP/ca.pem" \
	--request POST \
	--header "Content-Type: application/json" \
	--header "X-AnixOps-Redaction-Secret: anixops-header-secret-task-2" \
	--data '{"from":"redaction","secret":"anixops-body-secret-task-2"}' \
	--dump-header "$TMP/redaction-headers.out" \
	--output "$TMP/redaction-body.out" \
	"https://google.com:${ORIGIN_PORT}/contract/request-response?redaction=1"

grep -F "HTTP/1.1 200 OK" "$TMP/redaction-headers.out" >/dev/null
grep -i -F "X-AnixOps-Static-Response: static-response" "$TMP/redaction-headers.out" >/dev/null
node - "$TMP/redaction-body.out" <<'JS'
const fs = require("node:fs");
const body = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
if (body.code !== 7) throw new Error("redacted runner failure did not preserve static response body mutation");
JS
grep -F "script runner failed" "$TMP/shim.log" >/dev/null
if grep -F "anixops-body-secret-task-2" "$TMP/shim.log" >/dev/null; then
	echo "runner body secret leaked into shim log" >&2
	exit 1
fi
if grep -F "anixops-header-secret-task-2" "$TMP/shim.log" >/dev/null; then
	echo "runner header secret leaked into shim log" >&2
	exit 1
fi

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

run_raw_request_overflow_case() {
	python3 - "$TMP/raw-request-overflow.bin" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
path.write_bytes(b"q" * (32 * 1024 * 1024 + 2))
PY

	curl --silent --show-error --max-time 30 --http1.1 \
		--proxy "http://127.0.0.1:${SHIM_PORT}" \
		--cacert "$TMP/ca.pem" \
		--request POST \
		--header "Content-Type: application/octet-stream" \
		--data-binary "@${TMP}/raw-request-overflow.bin" \
		--dump-header "$TMP/raw-request-overflow-headers.out" \
		--output "$TMP/raw-request-overflow-body.out" \
		"https://google.com:${ORIGIN_PORT}/contract/request-response?raw-request-overflow=1&summary=1"

	grep -F "HTTP/1.1 201 Created" "$TMP/raw-request-overflow-headers.out" >/dev/null
	grep -i -F "X-AnixOps-Script: applied" "$TMP/raw-request-overflow-headers.out" >/dev/null

	node - "$TMP/raw-request-overflow-body.out" <<'JS'
const fs = require("node:fs");
const maxBodyRewriteBuffer = 32 * 1024 * 1024;
const body = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
if (body.ok !== true || !body.original) {
  throw new Error("raw overflow request did not reach the origin through the response contract");
}
if (body.original.requestEncoding !== "") {
  throw new Error(`raw overflow request changed Content-Encoding: ${body.original.requestEncoding}`);
}
if (body.original.requestByteCount !== maxBodyRewriteBuffer + 2) {
  throw new Error(`raw overflow request byte count mismatch: ${body.original.requestByteCount}`);
}
if (body.original.requestContentLength !== maxBodyRewriteBuffer + 2) {
  throw new Error(`raw overflow request Content-Length mismatch: ${body.original.requestContentLength}`);
}
if (body.original.requestStaticHeader !== "") {
  throw new Error("raw overflow request did not preserve its original headers");
}
if (body.original.requestAcceptEncoding !== "") {
  throw new Error(`raw overflow request changed implicit Accept-Encoding: ${body.original.requestAcceptEncoding}`);
}
JS
}

run_chunked_raw_request_overflow_case() {
	python3 - "$TMP/ca.pem" "$SHIM_PORT" "$ORIGIN_PORT" "$TMP/chunked-raw-request-overflow-body.out" <<'PY'
import socket
import ssl
import sys

ca_path, shim_port, origin_port, output_path = sys.argv[1:]
timeout = 30
target = f"google.com:{origin_port}"

sock = socket.create_connection(("127.0.0.1", int(shim_port)), timeout=timeout)
sock.settimeout(timeout)
sock.sendall(
    f"CONNECT {target} HTTP/1.1\r\nHost: {target}\r\nProxy-Connection: keep-alive\r\n\r\n".encode("ascii")
)
connect_response = bytearray()
while b"\r\n\r\n" not in connect_response:
    chunk = sock.recv(4096)
    if not chunk:
        raise SystemExit("proxy closed CONNECT before response")
    connect_response.extend(chunk)
if not connect_response.startswith(b"HTTP/1.1 200"):
    raise SystemExit(f"unexpected CONNECT response: {connect_response!r}")

context = ssl.create_default_context(cafile=ca_path)
tls = context.wrap_socket(sock, server_hostname="google.com")
tls.settimeout(timeout)
tls.sendall(
    (
        "POST /contract/request-response?chunked-raw-request-overflow=1&summary=1 HTTP/1.1\r\n"
        f"Host: {target}\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Accept-Encoding: gzip\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Trailer: X-AnixOps-Request-Trailer\r\n"
        "\r\n"
    ).encode("ascii")
)

remaining = 32 * 1024 * 1024 + 2
chunk = b"q" * (64 * 1024)
while remaining:
    payload = chunk[:min(len(chunk), remaining)]
    tls.sendall(f"{len(payload):x}\r\n".encode("ascii"))
    tls.sendall(payload)
    tls.sendall(b"\r\n")
    remaining -= len(payload)
tls.sendall(b"0\r\nX-AnixOps-Request-Trailer: preserved\r\n\r\n")

reader = tls.makefile("rb")
status = reader.readline()
if not status.startswith(b"HTTP/1.1 201"):
    raise SystemExit(f"unexpected raw relay response status: {status!r}")
headers = {}
while True:
    line = reader.readline()
    if line in (b"\r\n", b"\n", b""):
        break
    name, value = line.decode("iso-8859-1").split(":", 1)
    headers[name.lower()] = value.strip()

if "content-length" in headers:
    body = reader.read(int(headers["content-length"]))
elif headers.get("transfer-encoding", "").lower() == "chunked":
    chunks = []
    while True:
        line = reader.readline()
        size = int(line.split(b";", 1)[0], 16)
        if size == 0:
            while reader.readline() not in (b"\r\n", b"\n", b""):
                pass
            break
        chunks.append(reader.read(size))
        if reader.read(2) != b"\r\n":
            raise SystemExit("malformed chunked response")
    body = b"".join(chunks)
else:
    body = reader.read()

with open(output_path, "wb") as output:
    output.write(body)
reader.close()
tls.close()
PY

	node - "$TMP/chunked-raw-request-overflow-body.out" <<'JS'
const fs = require("node:fs");
const maxBodyRewriteBuffer = 32 * 1024 * 1024;
const body = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
if (body.ok !== true || !body.original) {
  throw new Error("chunked raw overflow request did not reach the origin through the response contract");
}
if (body.original.requestByteCount !== maxBodyRewriteBuffer + 2) {
  throw new Error(`chunked raw overflow request byte count mismatch: ${body.original.requestByteCount}`);
}
if (body.original.requestContentLength !== -1) {
  throw new Error(`chunked raw overflow Content-Length mismatch: ${body.original.requestContentLength}`);
}
if (body.original.requestTransferEncoding !== "chunked") {
  throw new Error(`chunked raw overflow transfer encoding mismatch: ${body.original.requestTransferEncoding}`);
}
if (body.original.requestTrailer !== "preserved") {
  throw new Error(`chunked raw overflow trailer mismatch: ${body.original.requestTrailer}`);
}
if (body.original.requestStaticHeader !== "") {
  throw new Error("chunked raw overflow request did not preserve its original headers");
}
if (body.original.requestAcceptEncoding !== "gzip") {
  throw new Error(`chunked raw overflow changed upstream Accept-Encoding: ${body.original.requestAcceptEncoding}`);
}
JS
}

run_raw_response_overflow_case() {
	curl --silent --show-error --max-time 30 --http1.1 \
		--proxy "http://127.0.0.1:${SHIM_PORT}" \
		--cacert "$TMP/ca.pem" \
		--header "Accept-Encoding: gzip" \
		--dump-header "$TMP/raw-response-overflow-headers.out" \
		--output "$TMP/raw-response-overflow-body.out" \
		"https://google.com:${ORIGIN_PORT}/contract/request-response?raw-response-overflow=1&raw-response-trailer=1"

	grep -F "HTTP/1.1 200 OK" "$TMP/raw-response-overflow-headers.out" >/dev/null
	if grep -i -F "X-AnixOps-Static-Response:" "$TMP/raw-response-overflow-headers.out" >/dev/null; then
		echo "raw overflow response unexpectedly applied static header mutation" >&2
		exit 1
	fi
	if grep -i -F "X-AnixOps-Script:" "$TMP/raw-response-overflow-headers.out" >/dev/null; then
		echo "raw overflow response unexpectedly dispatched response script" >&2
		exit 1
	fi

	python3 - "$TMP/raw-response-overflow-body.out" "$TMP/raw-response-overflow-headers.out" <<'PY'
import pathlib
import sys

max_body_rewrite_buffer = 32 * 1024 * 1024
body = pathlib.Path(sys.argv[1]).read_bytes()
headers = pathlib.Path(sys.argv[2]).read_text(encoding="iso-8859-1").lower()
expected_size = max_body_rewrite_buffer + 2
if len(body) != expected_size:
    raise SystemExit(f"raw overflow response size mismatch: {len(body)}")
if body != b"r" * expected_size:
    raise SystemExit("raw overflow response bytes changed")
if f"x-origin-body-length: {expected_size}" not in headers:
    raise SystemExit("raw overflow response metadata missing")
if "x-origin-accept-encoding: gzip" not in headers:
    raise SystemExit("raw overflow response changed upstream Accept-Encoding negotiation")
if "x-origin-relay-trailer: preserved" not in headers:
    raise SystemExit("raw overflow response trailer was not relayed")
PY
}

run_binary_body_case() {
	python3 - "$TMP/binary-body.raw" "$TMP/binary-body.gz" <<'PY'
import gzip
import pathlib
import sys

body = b"anixops\x00binary\xff\xc3\x28\nbody"
pathlib.Path(sys.argv[1]).write_bytes(body)
pathlib.Path(sys.argv[2]).write_bytes(gzip.compress(body))
PY

	curl --silent --show-error --max-time 12 --http1.1 \
		--proxy "http://127.0.0.1:${SHIM_PORT}" \
		--cacert "$TMP/ca.pem" \
		--request POST \
		--header "Content-Type: application/octet-stream" \
		--header "Content-Encoding: gzip" \
		--data-binary "@${TMP}/binary-body.gz" \
		--dump-header "$TMP/binary-body-headers.out" \
		--output "$TMP/binary-body.out" \
		"https://google.com:${ORIGIN_PORT}/contract/binary-body"

	grep -F "HTTP/1.1 200 OK" "$TMP/binary-body-headers.out" >/dev/null
	if grep -i -F "Content-Encoding:" "$TMP/binary-body-headers.out" >/dev/null; then
		echo "binary body response retained compressed representation" >&2
		sed -n '1,120p' "$TMP/binary-body-headers.out" >&2
		exit 1
	fi

	python3 - "$TMP/binary-body.raw" "$TMP/binary-body.out" "$TMP/binary-body-headers.out" <<'PY'
import hashlib
import pathlib
import sys

expected = pathlib.Path(sys.argv[1]).read_bytes()
actual = pathlib.Path(sys.argv[2]).read_bytes()
headers = pathlib.Path(sys.argv[3]).read_text(encoding="iso-8859-1")
metadata = {}
for line in headers.splitlines():
    if ":" not in line:
        continue
    key, value = line.split(":", 1)
    metadata[key.strip().lower()] = value.strip()
if metadata.get("x-origin-body-length") != str(len(expected)):
    raise SystemExit("binary origin length metadata mismatch")
if metadata.get("x-origin-body-sha256") != hashlib.sha256(expected).hexdigest():
    raise SystemExit("binary origin digest metadata mismatch")
if actual != expected:
    raise SystemExit("binary body did not survive request/response policy dispatch")
PY
}

run_compressed_response_case gzip
run_compressed_response_case deflate
run_compressed_request_case gzip
run_compressed_request_case deflate
run_compressed_request_overflow_case
run_raw_request_overflow_case
run_chunked_raw_request_overflow_case
run_raw_response_overflow_case
run_binary_body_case

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
echo "script contract: raw request and response overflow relay original bytes and headers fail-open"
echo "script contract: raw chunked request overflow relays transfer framing and trailers"
echo "script contract: binary bodies retain exact bytes through request/response policy dispatch"
echo "script contract: binary bodies bypass request and response script dispatch"
echo "script contract: runner failure logs use a redacted fixed classification"
echo "mitm_anixops script contract e2e passed"
