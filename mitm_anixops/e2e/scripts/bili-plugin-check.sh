#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
WORKSPACE=$(CDPATH= cd -- "$ROOT/.." && pwd)

BILI_PLUGIN=${BILI_PLUGIN:-"$ROOT/tests/fixtures/BiliBili.Enhanced.plugin"}
BILI_PLUGIN_SHA256=${BILI_PLUGIN_SHA256:-"e0eca33cd58a765283e863924030187448665e39397b1231e0dcc3db60429f52"}
BILI_SCRIPT_URL=${BILI_SCRIPT_URL:-"https://github.com/BiliUniverse/Enhanced/releases/download/v0.5.13/response.bundle.js"}
BILI_SCRIPT_SHA256=${BILI_SCRIPT_SHA256:-"f34238dce8fc4c525a371e934382d2241a6712a366de609892c710712808f445"}
BILI_SCRIPT=${BILI_SCRIPT:-"$WORKSPACE/downloads/BiliBili.Enhanced.response.bundle.js"}
BILI_SNIPPET_URL=${BILI_SNIPPET_URL:-"https://github.com/BiliUniverse/Enhanced/releases/download/v0.5.13/BiliBili.Enhanced.snippet"}
BILI_SNIPPET_SHA256=${BILI_SNIPPET_SHA256:-"1f6a5799ba49a34e467cde102073880aff4fee9bf79d3e33ca60e100b1301931"}
BILI_SNIPPET=${BILI_SNIPPET:-"$WORKSPACE/downloads/BiliBili.Enhanced.snippet"}
BILI_SGMODULE_URL=${BILI_SGMODULE_URL:-"https://github.com/BiliUniverse/Enhanced/releases/download/v0.5.13/BiliBili.Enhanced.sgmodule"}
BILI_SGMODULE_SHA256=${BILI_SGMODULE_SHA256:-"f510220000bfe99221116038146f3af96c5f2a1f4c1f3d164c70ca3cabc02240"}
BILI_SGMODULE=${BILI_SGMODULE:-"$WORKSPACE/downloads/BiliBili.Enhanced.sgmodule"}

TMP=${TMPDIR:-/tmp}/anixops-bili-e2e-$$
mkdir -p "$TMP"
cleanup() {
	rm -rf "$TMP"
}
trap cleanup EXIT INT TERM

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

verify_sha_file() {
	path=$1
	expected_sha=$2
	label=$3
	actual_sha=$(sha256sum "$path" | awk '{print $1}')
	if [ "$actual_sha" != "$expected_sha" ]; then
		echo "unexpected $label sha256: $actual_sha" >&2
		echo "expected: $expected_sha" >&2
		exit 1
	fi
}

ensure_sha_file() {
	path=$1
	url=$2
	expected_sha=$3
	label=$4
	if [ ! -f "$path" ]; then
		mkdir -p "$(dirname "$path")"
		curl -L -o "$path" "$url"
	fi
	verify_sha_file "$path" "$expected_sha" "$label"
}

if [ ! -f "$BILI_PLUGIN" ]; then
	echo "missing Bili plugin fixture: $BILI_PLUGIN" >&2
	exit 1
fi

verify_sha_file "$BILI_PLUGIN" "$BILI_PLUGIN_SHA256" "BiliBili.Enhanced.plugin"
ensure_sha_file "$BILI_SCRIPT" "$BILI_SCRIPT_URL" "$BILI_SCRIPT_SHA256" "response.bundle.js"
ensure_sha_file "$BILI_SNIPPET" "$BILI_SNIPPET_URL" "$BILI_SNIPPET_SHA256" "BiliBili.Enhanced.snippet"
ensure_sha_file "$BILI_SGMODULE" "$BILI_SGMODULE_URL" "$BILI_SGMODULE_SHA256" "BiliBili.Enhanced.sgmodule"

make -C "$ROOT" all >/dev/null
cc -I"$ROOT/include" -std=c99 -Wall -Wextra -Werror \
	"$ROOT/e2e/script_runtime/match_dump.c" \
	"$ROOT/build/libmitm_anixops.a" \
	-o "$ROOT/build/bili_script_match_dump"

URL="https://app.bilibili.com/x/resource/show/tab/v2?build=1"
"$ROOT/build/bili_script_match_dump" "$BILI_PLUGIN" "$URL" > "$TMP/plugin.match.out"

assert_contains "$TMP/plugin.match.out" "kind=2"
assert_contains "$TMP/plugin.match.out" "phase=1"
assert_contains "$TMP/plugin.match.out" "requires_body=1"
assert_contains "$TMP/plugin.match.out" "response.bundle.js"
assert_contains "$TMP/plugin.match.out" "Home.Switch=true"
assert_contains "$TMP/plugin.match.out" "Storage=Argument"
assert_contains "$TMP/plugin.match.out" "LogLevel=WARN"

"$ROOT/build/bili_script_match_dump" "$BILI_SNIPPET" "$URL" > "$TMP/snippet.match.out"
assert_contains "$TMP/snippet.match.out" "kind=2"
assert_contains "$TMP/snippet.match.out" "phase=1"
assert_contains "$TMP/snippet.match.out" "requires_body=1"
assert_contains "$TMP/snippet.match.out" "response.bundle.js"
assert_contains "$TMP/snippet.match.out" "argument="

"$ROOT/build/bili_script_match_dump" "$BILI_SGMODULE" "$URL" > "$TMP/sgmodule.match.out"
assert_contains "$TMP/sgmodule.match.out" "kind=2"
assert_contains "$TMP/sgmodule.match.out" "phase=1"
assert_contains "$TMP/sgmodule.match.out" "requires_body=1"
assert_contains "$TMP/sgmodule.match.out" "response.bundle.js"
assert_contains "$TMP/sgmodule.match.out" 'Home.Switch="true"'
assert_contains "$TMP/sgmodule.match.out" 'Storage="Argument"'
assert_contains "$TMP/sgmodule.match.out" 'LogLevel="WARN"'

ARGUMENT=$(sed -n 's/^argument=//p' "$TMP/plugin.match.out")
if [ -z "$ARGUMENT" ]; then
	echo "missing argument from match output" >&2
	sed -n '1,160p' "$TMP/plugin.match.out" >&2
	exit 1
fi

run_bundle() {
	name=$1
	url=$2
	body=$3
	printf '%s\n' "$body" > "$TMP/${name}.origin.json"
	"$ROOT/build/bili_script_match_dump" "$BILI_PLUGIN" "$url" > "$TMP/${name}.match.out"
	case_argument=$(sed -n 's/^argument=//p' "$TMP/${name}.match.out")
	if [ -z "$case_argument" ]; then
		echo "missing argument for $name" >&2
		sed -n '1,160p' "$TMP/${name}.match.out" >&2
		exit 1
	fi
	node "$ROOT/e2e/script_runtime/anixops_runner.js" \
		--script "$BILI_SCRIPT" \
		--url "$url" \
		--argument "$case_argument" \
		--body "$TMP/${name}.origin.json" \
		--responseHeaders '{"Content-Type":"application/json"}' \
		> "$TMP/${name}.done.json" \
		2> "$TMP/${name}.script.log"
}

run_bundle tab "$URL" '{"code":0,"message":"0","data":{}}'
run_bundle mine "https://app.bilibili.com/x/v2/account/mine?build=1" '{"code":0,"message":"0","data":{}}'
run_bundle ipad "https://app.bilibili.com/x/v2/account/mine/ipad?build=1" '{"code":0,"message":"0","data":{}}'
run_bundle region "https://app.bilibili.com/x/v2/region/index?build=1" '{"code":0,"message":"0","data":[]}'
run_bundle channel "https://app.bilibili.com/x/v2/channel/region/list?build=1" '{"code":0,"message":"0","data":[]}'

node - "$TMP/tab.done.json" <<'JS'
const fs = require("node:fs");
const result = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
if (!result || typeof result.body !== "string") {
  throw new Error("$done result did not include response body");
}
const body = JSON.parse(result.body);
if (!body.data || !Array.isArray(body.data.tab) || body.data.tab.length !== 7) {
  throw new Error(`unexpected tab count: ${body.data && body.data.tab && body.data.tab.length}`);
}
if (!Array.isArray(body.data.top) || body.data.top.length !== 1) {
  throw new Error(`unexpected top count: ${body.data && body.data.top && body.data.top.length}`);
}
if (!Array.isArray(body.data.bottom) || body.data.bottom.length !== 5) {
  throw new Error(`unexpected bottom count: ${body.data && body.data.bottom && body.data.bottom.length}`);
}
if (!body.data.top_left || body.data.top_left.url !== "bilibili://user_center/") {
  throw new Error("unexpected top_left rewrite");
}
const selected = body.data.tab.find((item) => item.default_selected === 1);
if (!selected || selected.uri !== "bilibili://pegasus/promo") {
  throw new Error("default tab was not selected as expected");
}
JS

node - "$TMP/mine.done.json" <<'JS'
const fs = require("node:fs");
const result = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
const body = JSON.parse(result.body);
if (!body.data || !Array.isArray(body.data.sections_v2) || body.data.sections_v2.length !== 4) {
  throw new Error(`unexpected mine section count: ${body.data && body.data.sections_v2 && body.data.sections_v2.length}`);
}
const firstItems = body.data.sections_v2[0].items || [];
if (!firstItems.some((item) => item.uri === "bilibili://user_center/download")) {
  throw new Error("mine response did not include expected download service");
}
JS

node - "$TMP/ipad.done.json" <<'JS'
const fs = require("node:fs");
const result = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
const body = JSON.parse(result.body);
if (!body.data || !Array.isArray(body.data.ipad_upper_sections)) {
  throw new Error("ipad response missing ipad_upper_sections");
}
if (!Array.isArray(body.data.ipad_recommend_sections) || body.data.ipad_recommend_sections.length !== 6) {
  throw new Error(`unexpected ipad recommend count: ${body.data && body.data.ipad_recommend_sections && body.data.ipad_recommend_sections.length}`);
}
if (!Array.isArray(body.data.ipad_more_sections) || body.data.ipad_more_sections.length !== 2) {
  throw new Error(`unexpected ipad more count: ${body.data && body.data.ipad_more_sections && body.data.ipad_more_sections.length}`);
}
JS

node - "$TMP/region.done.json" <<'JS'
const fs = require("node:fs");
const result = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
const body = JSON.parse(result.body);
if (!Array.isArray(body.data) || body.data.length !== 40) {
  throw new Error(`unexpected region count: ${body.data && body.data.length}`);
}
if (!body.data[0] || body.data[0].tid !== 1 || body.data[0].goto !== "0") {
  throw new Error("region response did not preserve full region item shape");
}
if (!Array.isArray(body.data[0].children) || !Array.isArray(body.data[0].config)) {
  throw new Error("region response missing children/config");
}
JS

node - "$TMP/channel.done.json" <<'JS'
const fs = require("node:fs");
const result = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
const body = JSON.parse(result.body);
if (!Array.isArray(body.data) || body.data.length !== 40) {
  throw new Error(`unexpected channel region count: ${body.data && body.data.length}`);
}
if (!body.data[0] || body.data[0].tid !== 1 || body.data[0].goto !== "") {
  throw new Error("channel region response did not normalize goto");
}
if ("children" in body.data[0] || "config" in body.data[0]) {
  throw new Error("channel region response should remove children/config");
}
JS

echo "plugin fixture: $BILI_PLUGIN"
echo "snippet fixture: $BILI_SNIPPET"
echo "sgmodule fixture: $BILI_SGMODULE"
echo "script bundle: $BILI_SCRIPT"
echo "script dispatch: BiliBili.Enhanced plugin, snippet, and sgmodule matched"
echo "script execution: modified tab, mine, ipad mine, region, and channel region responses"
echo "BiliUniverse Enhanced plugin e2e passed"
