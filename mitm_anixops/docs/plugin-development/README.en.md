# MITM Plugin Development Guide

[中文](README.md)

This guide is for third-party developers who want to write MITM plugins,
rewrite rules, and scripts for the AnixOps Net client. It covers which Loon /
Surge / Quantumult X syntax the client accepts, the script `$` API, local
debugging, the recipe format, and how to submit and contribute.

It reflects the `mitm_anixops` and `networkcore_anixops` main branches as of
2026-11-13. Per-capability status lives in
[`../compatibility/matrix.md`](../compatibility/matrix.md) and in the support
report the client shows when a plugin is imported. Where this guide disagrees
with them, they win, and a pull request fixing this guide is welcome.

The Chinese version is the primary text; this is a translation.

## Contents

1. [Read first: terms and prohibited uses](#1-read-first-terms-and-prohibited-uses)
2. [How a plugin runs](#2-how-a-plugin-runs)
3. [Supported plugin formats](#3-supported-plugin-formats)
4. [Remote resources: scripts must use https](#4-remote-resources-scripts-must-use-https)
5. [Script `$` API](#5-script--api)
6. [Recipe format](#6-recipe-format)
7. [Fixtures, contracts, and matrix rows](#7-fixtures-contracts-and-matrix-rows)
8. [Local debugging](#8-local-debugging)
9. [Complete example: ipleak IP rewrite](#9-complete-example-ipleak-ip-rewrite)
10. [Submission and contribution](#10-submission-and-contribution)

## 1. Read first: terms and prohibited uses

Using, distributing, and writing plugins for this component is governed by
[`TERMS.md`](../../TERMS.md) (Chinese). The parts that matter most to plugin
authors are the product-specific terms and section 3, prohibited uses:

- **Only process traffic you are entitled to process.** Plugins may be used
  on your own devices and traffic, or in a test environment where the device
  owner and the parties to the communication have given written permission.
- **No recipes or plugins that defeat security mechanisms.** We do not
  publish, recommend, or list in
  [`../recipes/external-examples.md`](../recipes/external-examples.md) any
  content that defeats a third-party service's security mechanisms: tampering
  with game settlement or values, evading anti-cheat, forging payments or
  orders, or bypassing authentication, risk control, DRM, or certificate
  validation. Listed content found to do so is removed.
- **The root certificate is generated and trusted by the user, manually.**
  A plugin cannot ask the client to install a certificate silently, and
  `[MITM]` lists only the hosts the plugin actually needs.
- **No exfiltration.** Decrypted traffic can contain credentials and personal
  data. Scripts must not send cookies, tokens, accounts, or bodies to third
  parties, and must not log them.

Plugins that break these terms are rejected in review, see
[section 10](#10-submission-and-contribution).

## 2. How a plugin runs

| Layer | Responsibility | Code |
| --- | --- | --- |
| Importer | Reads Loon `.plugin`, Surge `.sgmodule`, Quantumult X `.snippet` / rewrite snippets; grades every line; translates each dialect into core grammar; lists remote files to download | `networkcore_anixops` `crates/mitm-policy` (`loon_plugin.rs`) |
| Policy core | This repository's C library: which hosts to decrypt, URL rewrite, header rewrite, body rewrite, script dispatch metadata | this repository, `src/`, `include/mitm_anixops.h` |
| Script runtime | Runs `[Script]` JavaScript and provides the `$` API | `networkcore_anixops` `crates/script-runtime` |

Every imported line is graded `supported`, `partial` (a documented subset),
`deferred` (handed to the host, for example scheduled scripts), or
`unsupported` (that line has no effect; the rest of the plugin still works).
The client shows these counts and a per-line reason. **Treat that report as
the first answer to "does my plugin work on AnixOps".**

Order within one HTTP exchange (see
[`../script_runtime_contract.md`](../script_runtime_contract.md#adapter-ordering)):

- request: MITM decision → URL rewrite (redirect or reject ends the pipeline)
  → request header rewrite → request body rewrite → request script → upstream;
- response: decode and dechunk → response header rewrite → response body
  rewrite → response script → reframe and return to the app.

**Scripts always run after the static rules** and see the statically
rewritten message.

## 3. Supported plugin formats

### 3.1 Files and metadata

- Accepted: Loon `.plugin`, Surge `.sgmodule`, Quantumult X `.snippet` and
  rewrite snippets (including commented headers such as `#[rewrite_local]`,
  or no headers at all), and portable `.conf`.
- One plugin text is at most 8 MiB.
- Header metadata: `#!name`, `#!desc`, `#!author`, `#!homepage`, `#!icon`,
  `#!category`, and `#!arguments` are recognized and shown; other `#!` lines
  (for example Surge `#!requirement`) are tolerated and ignored. Metadata has
  no policy effect. Recipe metadata `#!recipe*` is covered in
  [section 6](#6-recipe-format).

### 3.2 Sections

The high-frequency subset. The full surface, tests, and gaps are in
[`../compatibility/matrix.md`](../compatibility/matrix.md).

| Section | Accepted syntax | Notes | Contract |
| --- | --- | --- | --- |
| `[Argument]` | `Key = input,default,tag=...,desc=...`, `switch`, `select` | Referenced as `{Key}` or `argument=[{A},{B}]` | [loon-argument-section](../compatibility/loon-argument-section.md) |
| `[MITM]` | `hostname = a.example.com, *.example.com, -deny.example.com`, `%APPEND%` | Only listed hosts are decrypted; `*.example.com` does not include `example.com`, list both when needed | [loon-mitm-options](../compatibility/loon-mitm-options.md) |
| `[Rewrite]` / `[URL Rewrite]` | `<url regex> <target> 301/302/303/307/308`, `reject`, `reject-200`, `reject-img`, `reject-dict`, `reject-array`, `reject-NNN`, ... | Surge `<url> <replacement> <type>` and QX `<url> url <action>` are translated | [request-rewrite-common](../compatibility/request-rewrite-common.md) |
| `[Header Rewrite]` | `header-add`, `header-replace`, `header-del`, `header-replace-regex`; Surge `http-request` / `http-response` prefixed lines | Case-insensitive names; `Set-Cookie` handled as independent fields | [header-mutation-common](../compatibility/header-mutation-common.md) |
| `[Body Rewrite]` | Loon `http-response <url regex> <find regex> <replacement> [<find> <replacement> ...]`, `http-request ...`; `response-body-json-replace $.path value`; `http-response-jq <url regex> '<jq filter>'`, `http-request-jq ...` | Pairs apply in order; jq runs in the client with jaq (Rust) | [body-mutation-common](../compatibility/body-mutation-common.md) |
| `[Script]` | `http-request` / `http-response <url regex> script-path=https://...,requires-body=true,binary-body-mode=false,max-size=...,timeout=...,tag=...,argument=...,enable=...` | See section 5 | [loon-script-metadata](../compatibility/loon-script-metadata.md) |
| `[Map Local]` | `<url regex> data="..."` or `data=https://...`, `mock-response-body` | Answered without the origin; QX `echo-response` is translated here | [response-rewrite-common](../compatibility/response-rewrite-common.md) |
| `[Rule]` | `URL-REGEX`, `DOMAIN`, `DOMAIN-SUFFIX`, `DOMAIN-KEYWORD` with `REJECT` and other policies | Feeds the client routing model; `FINAL` inside a plugin is `unsupported` | [loon-rule-reject](../compatibility/loon-rule-reject.md), [loon-rule-routing-guard](../compatibility/loon-rule-routing-guard.md) |
| Scheduled and event scripts | `cron "<expr>" script-path=...`, `network-changed`, `generic`, QX `[task_local]` | Leave the HTTP pipeline and go to the host scheduler; per-platform support is shown in the import report | [cron-task-trigger](../compatibility/cron-task-trigger.md) |

Regex dialect: the client's core is built with PCRE2, so common PCRE syntax
(`(?i)`, `\d`, non-capturing and named groups, lazy quantifiers) works;
replacements accept `$1`, `${1}`, `\1`, `${name}`, `$<name>`. This
repository's default build is POSIX ERE plus a PCRE-compatible subset, so
build with `PCRE2=1` when debugging locally (section 8).

### 3.3 Not supported or refused

- `[MITM] skip-server-cert-verify = true` is refused (`unsupported`). The
  client does not turn off upstream certificate validation for a plugin.
- Lines with no implementation yet, such as header-block regexes, are graded
  `unsupported` and kept away from the core.
- Host-specific APIs such as Surge `$httpAPI` / `$environment`, Quantumult X
  `$utils`, and Shadowrocket `$rocket` are not provided (section 5).

## 4. Remote resources: scripts must use https

The client downloads `script-path=` and remote `[Map Local]` `data=` URLs:

- **`https://` only.** `http://` URLs are refused, and so is an https URL
  that redirects to http.
- No cookies, no cache. One script is at most 4 MiB and one plugin at most
  8 MiB; enabled plugins and enabled scripts are each capped at 8 MiB in
  total; remote `[Map Local]` data is read at start, 2 MiB in total.
- **The user allows each script before it first runs.** The import report
  lists every remote file a plugin needs.
- Publish pinned URLs (a commit SHA or a release tag), never a moving branch.
  If the content behind a URL changes, users no longer run what they reviewed.

`script-path` must be a remote https URL. For local debugging, map it to a
local file with the runner's `--script-map` (section 8).

## 5. Script `$` API

### 5.1 Environment

- Engine: QuickJS-ng on desktop and iOS; a JavaScriptCore binding exists for
  Apple platforms and passes the same conformance suite.
- Scripts run as sloppy-mode global code, with Promise, `async`/`await`, and
  timers.
- No file system, process, or network access, except `$httpClient`.
- Each dispatch gets a fresh runtime; scripts share state only through
  `$persistentStore`.

### 5.2 Globals

Source of truth:
[`crates/script-runtime`](https://github.com/AnixOps/networkcore_anixops/tree/main/crates/script-runtime)
in `networkcore_anixops`.

| Global | Behavior |
| --- | --- |
| `$request` | `url`, `method`, `headers` (object); UTF-8 string `body` when `requires-body=true` |
| `$response` | `status`, `statusCode` (same value, for QX scripts), `headers`; `body` when `requires-body=true` |
| `$argument` | The line's `argument=` value, **a string**. `argument=[{A},{B}]` expands to `A=value&B=value` |
| `$done(value)` | Ends the script; **the first call wins**. Fields in 5.3 |
| `$persistentStore.read(key)` / `.write(value, key)` / `.remove(key)` | Persistent key-value store. Key ≤ 256 B, value ≤ 512 KiB, total ≤ 8 MiB. A missing key uses `networkcore.script.<tag>` |
| `$prefs.valueForKey` / `setValueForKey` / `removeValueForKey` | QX names for the same store; `removeAllValues` returns `false` |
| `$notification.post(title, subtitle, content, attach)` | `attach` is a URL string or `{openUrl}` / `{"open-url"}` / `{url}`; at most 16 per dispatch |
| `$notify(title, subtitle, content, options)` | QX name for `$notification.post` |
| `$httpClient.get/post/put/delete/head/options/patch(options, callback)` | `callback(error, response, body)`; `response` has `status`, `statusCode`, `headers`. See 5.4 |
| `$task.fetch(options)` | QX; a Promise resolving `{statusCode, status, headers, body}` or rejecting `{error}`. **Defined only when the script uses `$task` and never mentions `$loon`**, so cross-client helpers such as Env.js take their Loon branch |
| `$loon` | The string `NetworkCore/<version>`, for environment detection |
| `$script` | `name`, `type` (`http-request` / `http-response`), `startTime` |
| `$event` | `{name}` in event scripts |
| `console.log/info/warn/error/debug` | Collected, at most 128 lines of 2 KiB per dispatch |
| `setTimeout` / `clearTimeout` | Run inside the dispatch budget, at most 64 |

Env.js-style helpers detect AnixOps as **Loon**. `$environment`, `$httpAPI`,
`$utils`, and `$rocket` are undefined; guard those branches with `typeof`.

### 5.3 `$done` fields

Request scripts (`http-request`):

```javascript
// Rewrite the request and continue to the origin
$done({ url: "https://example.com/new", headers: { "X-Feature": "on" }, body: "{}" });

// Answer without the origin
$done({ response: { status: 200, headers: { "Content-Type": "application/json" }, body: "{}" } });
```

Response scripts (`http-response`):

```javascript
$done({ status: 200, headers: { "Content-Type": "application/json" }, body: "{\"ok\":true}" });
```

- Every field is optional; a missing field means unchanged. `$done()` or
  `$done({})` passes the message through.
- An empty-string `body` replaces the body with an empty one.
- `status` may be a number or a status line.
- After a body change the client drops stale `Content-Encoding` and
  recomputes `Content-Length`.
- Bodies are strings by default. With `binary-body-mode=true`, `body` is a
  `Uint8Array` and QX-style `bodyBytes` (an ArrayBuffer) is also present;
  `$done` accepts `Uint8Array`, `ArrayBuffer`, or a string (encoded as UTF-8).

### 5.4 `$httpClient`

- Requests go through the client tunnel and are routed like any other
  connection of the device.
- HTTP/1.1 with `Connection: close`; **redirects are not followed**, 3xx is
  returned to the script.
- `options` is a URL string or `{url, headers, body, timeout}`; a non-string
  `body` is `JSON.stringify`-ed.
- At most 16 requests per dispatch; error messages are fixed strings without
  URLs or payloads.
- A host without a network path returns an error to the callback; handle
  `error`.

### 5.5 Budgets and failure handling

| Item | Limit |
| --- | --- |
| Body visible to rewrites and scripts | 1 MiB; larger bodies skip scripts and pass through |
| Per-line `max-size` | The script does not run on a larger body |
| Per-line `timeout` | Seconds, as in Loon / Surge; iOS caps it at 3 s, desktop defaults to 5 s with a 30 s hard cap |
| QuickJS heap | 8 MiB per dispatch; larger scripts get more by source size, up to 32 MiB |
| Concurrency | Heaps of concurrently running scripts total at most 32 MiB |

**On timeout, exception, budget breach, or an invalid `$done` shape, the
client passes the original message through** (fail-open) and records the
script's `tag`. Do not rely on "fail means block".

## 6. Recipe format

A recipe is a plugin file that composes supported syntax into one
reproducible outcome, plus `#!recipe*` metadata. It is data, not code: no new
parser branch and no C ABI change.

- Format: [`../recipes/recipe-spec-v1.md`](../recipes/recipe-spec-v1.md), with
  nine v1 keys: `#!recipe`, `#!recipe.id`, `#!recipe.version`,
  `#!recipe.scope`, `#!recipe.authority`, `#!recipe.strategy`,
  `#!recipe.guard`, `#!recipe.evidence`, `#!recipe.reviewed`.
- Procedure, review checklist, and template:
  [`../recipes/adding-a-recipe.md`](../recipes/adding-a-recipe.md).
- Parser handling: contract
  [`../compatibility/recipe-metadata.md`](../compatibility/recipe-metadata.md),
  matrix row "Recipe module metadata" (PR #48). The keys are tolerated as
  `Plugin` diagnostics with no policy behavior; `anixops-mitm-runner scan`
  reports `recipeWarnings` for missing keys, invalid values, a missing guard,
  and a mismatch between `#!recipe.scope` and `[MITM] hostname`. Warnings
  never reject the file.
- Recipes for concrete services live in their authors' repositories and are
  registered in
  [`../recipes/external-examples.md`](../recipes/external-examples.md). The
  prohibited categories in section 1 are not registered.

Plain plugins without `#!recipe*` metadata work the same; the recipe format
only makes a plugin easier to review and reproduce.

## 7. Fixtures, contracts, and matrix rows

To make this repository accept a Loon / Surge / QX shape it does not accept
yet, follow [`../../CONTRIBUTING.md`](../../CONTRIBUTING.md) and submit, **in
one pull request**:

1. **Contract** in `docs/compatibility/`: source ecosystem, accepted syntax,
   parser output, positive and negative cases, unimplemented items. Rules in
   [`../compatibility/source-contracts.md`](../compatibility/source-contracts.md).
2. **Fixtures**: `tests/fixtures/<Ecosystem>.<Name>.<ext>` and a malformed
   counterpart, for example `Loon.RecipeMetadata.plugin` and
   `Loon.RecipeMetadata.Unsupported.plugin`. Use reserved domains such as
   `*.example` and `*.test`.
3. **Tests**: register positive and negative `config/<name>_fixture_*` tests in
   `tests/test_config.c` (or the matching `tests/test_*.c`).
4. **Matrix row** in [`../compatibility/matrix.md`](../compatibility/matrix.md):
   ecosystem, status (`supported` / `partial` / `planned` / `unsupported`),
   contract, fixtures, positive and negative test IDs, gaps. A row becomes
   `supported` only after GitHub Actions proves its tests.
5. **CHANGELOG** entry under Unreleased.

`scripts/compatibility-evidence-check.sh` verifies that referenced fixtures
and test IDs exist and that every top-level fixture is documented.

PR #48 is a good model: contract `recipe-metadata.md`, three
`Loon.RecipeMetadata*.plugin` fixtures, tests
`config/loon_recipe_metadata_fixture_records_tolerated_keys` /
`config/loon_recipe_metadata_unsupported_keys_are_not_claimed`, and the
"Recipe module metadata" matrix row.

Importer problems (translating Loon / Surge / QX syntax) belong in
`networkcore_anixops`: fixtures in
`crates/mitm-policy/tests/fixtures/loon-plugins/`, contract tests in
`crates/mitm-policy/tests/*_contracts.rs`. `$` API behavior is pinned by
conformance cases `crates/script-runtime/tests/fixtures/conformance/<case>.js`
plus `<case>.json`, which must pass on both QuickJS and JavaScriptCore.

## 8. Local debugging

Local debugging is for developing your own plugin. For pull requests to this
repository, GitHub Actions is the acceptance result; a local build or test is
not acceptance evidence.

### 8.1 Check core grammar with the runner

```sh
# Match the client: PCRE2 regex + jq
make PCRE2=1 JQ=1 runner

# Parse: rule counts, ignored and rejected lines
build/anixops-mitm-runner scan my.plugin

# Match: which rules hit this URL in the response phase
build/anixops-mitm-runner trace --plugin my.plugin \
  --url https://api.example.test/v1/profile --phase response

# Negative: the request phase and out-of-scope hosts must not match
build/anixops-mitm-runner trace --plugin my.plugin \
  --url https://api.example.test/v1/profile --phase request
build/anixops-mitm-runner trace --plugin my.plugin \
  --url https://other.example.test/v1/profile --phase response

# Host decision: would this host be decrypted
build/anixops-mitm-runner trace --plugin my.plugin \
  --host api.example.test --mitm-enabled --cert-state trusted
```

The runner reads **core grammar**, url first and action second, for example
`^https://api\.example\.test/ response-body-replace-regex find replace`.
Action-first Loon `[Body Rewrite]` lines (`http-response <url> <find>
<replace>`), Surge `[URL Rewrite]` `<url> <replacement> <type>`, and QX
`url ...` lines are translated by the client importer before they reach the
core. For those, use the client's import report, or write the equivalent core
line to debug with the runner.

### 8.2 Replay scripts offline

```sh
build/anixops-mitm-runner replay --plugin my.plugin --fixture cases.tsv \
  --script-runner e2e/script_runtime/anixops_runner.js \
  --script-map https://example.com/my/response.js=./response.js \
  --script-store build/my-store.json \
  --timeout-ms 3000
```

`cases.tsv` has one case per line, tab-separated:

```text
case<TAB>name<TAB>request|response<TAB>url<TAB>body-or-
```

- `--script-map remote-url=local-file` maps a `script-path` to a local file,
  offline; `--script-bundle manifest.json` maps several files and checks
  sha256 digests.
- `--script-store` backs `$persistentStore` with a JSON file shared across
  invocations.
- Replaying the same input twice must give identical output; otherwise the
  script depends on hidden state.

The Node reference runner provides only `$request`, `$response`, `$argument`,
`$persistentStore`, `$done`, and `$script`. Verify scripts that use
`$httpClient`, `$notification`, `console`, timers, or QX APIs in the client.

### 8.3 Verify in the client

1. Host the plugin and scripts at https URLs (for example a GitHub raw link
   pinned to a commit SHA).
2. Import the plugin by URL and read the support report: grade and reason per
   line.
3. Generate and manually trust the root certificate yourself, turn on HTTPS
   decryption, enable the plugin, and allow its scripts.
4. Open the target page and check the result. The iOS client's developer mode
   (unlocked in Settings) shows diagnostics and events.
5. After changing a script, re-import from a new commit URL instead of relying
   on cache refresh.

## 9. Complete example: ipleak IP rewrite

This example changes the IPv4 address shown on [ipleak.net](https://ipleak.net/)
to `233.333.333.333` (deliberately impossible, so a working rewrite is
obvious) to confirm MITM and rewriting work. It changes only the page you
yourself see.

### 9.1 Find where the data comes from

The page shows the address from two places:

- the main page HTML itself: the IP details table and the `jsonData`
  attribute;
- HTML fragments from `ipv4.ipleak.net/?mode=ajax` and
  `ipv6.ipleak.net/?mode=ajax`.

The page does **not** read `/json/`, so a `/json/`-only rule changes nothing
on the page. The WebRTC section uses STUN, not HTTP, and cannot be rewritten
by MITM. Record traffic with an observe-only script first (see
[`../recipes/adding-a-recipe.md`](../recipes/adding-a-recipe.md#step-3-recon))
instead of guessing paths.

### 9.2 Static version (no script)

`ipleak-demo.sgmodule` (comments kept in Chinese as shipped):

```text
#!name = ipleak IP 改写演示
#!desc = 把 ipleak.net 页面显示的 IPv4 改成 233.333.333.333，用来确认 MITM 生效

[Body Rewrite]
# 页面上的 IP 来自首页 HTML（IP 详情表、jsonData）和 ipv4/ipv6.ipleak.net/?mode=ajax 返回的 HTML 片段，不走 /json/。
http-response ^https?://(ipv4\.|ipv6\.)?ipleak\.net/(\?.*)?$ [0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3} 233.333.333.333
# 直接打开 https://ipleak.net/json/ 时改 JSON 的 ip 字段。
http-response-jq ^https?://(ipv4\.|ipv6\.)?ipleak\.net/json/?(\?.*)?$ '.ip = "233.333.333.333"'

[MITM]
hostname = %APPEND% ipleak.net, *.ipleak.net
```

- The first `[Body Rewrite]` matches only the main page and the `?mode=ajax`
  fragments (path `/` with an optional query), never `/static/js/...`, and
  replaces every IPv4-shaped string in the body.
- The second uses jq to change only the JSON `ip` field; lookups of other
  addresses such as `/json/8.8.8.8` do not match.
- `[MITM]` lists both `ipleak.net` and `*.ipleak.net` because the wildcard
  does not include the apex; `%APPEND%` appends to the user's host list
  instead of replacing it.

The module and its behavior are pinned by the `networkcore_anixops` contract
test `ipleak_demo_rewrites_the_address_the_page_shows` (PR #224,
`crates/mitm-policy/tests/loon_plugin_p1_rewrite_contracts.rs`, fixture
`crates/mitm-policy/tests/fixtures/loon-plugins/ipleak-demo.sgmodule`). It
asserts that:

- the import grade is fully supported, and `ipleak.net`, `ipv4.ipleak.net`,
  and `ipv6.ipleak.net` are decrypted;
- `203.0.113.7` in the ajax fragment becomes `233.333.333.333`;
- the main page details table and `jsonData` are rewritten while non-IP text
  such as `jquery-4.0.0.min.js` and latitude `37.751` is untouched;
- the `/json/` `ip` field is rewritten and other fields are not;
- `/static/js/index.js` and `/json/8.8.8.8` are not rewritten.

That is the test shape a good plugin should have: positive cases prove it
changes what it should, negative cases prove it leaves everything else alone.

### 9.3 Script version (shows the `$` API)

The same `/json/` rewrite as a script, demonstrating `$argument`,
`$response`, `$persistentStore`, `console`, and `$done`.

`ipleak-script.plugin`:

```text
#!name = ipleak script rewrite demo
#!desc = Rewrite the ip field of ipleak.net/json/ to a configurable address

[Argument]
FakeIP = input,233.333.333.333,tag=Shown address,desc=Address shown after the rewrite

[Script]
http-response ^https?://(ipv4\.|ipv6\.)?ipleak\.net/json/?(\?.*)?$ script-path=https://raw.githubusercontent.com/<you>/<repo>/<commit-sha>/ipleak-json.js, requires-body=true, max-size=65536, timeout=3, tag=ipleak-json, argument=[{FakeIP}]

[MITM]
hostname = %APPEND% ipleak.net, *.ipleak.net
```

`ipleak-json.js`:

```javascript
// On AnixOps $argument is a string such as "FakeIP=233.333.333.333";
// other clients may pass an object, so handle both.
function readArguments(raw) {
  if (raw && typeof raw === "object") {
    return raw;
  }
  var out = {};
  String(raw || "").split("&").forEach(function (pair) {
    var i = pair.indexOf("=");
    if (i > 0) {
      out[pair.slice(0, i)] = pair.slice(i + 1);
    }
  });
  return out;
}

var args = readArguments(typeof $argument === "undefined" ? "" : $argument);
var fakeIP = args.FakeIP || "233.333.333.333";
var body = $response.body;

try {
  var data = JSON.parse(body);
  if (typeof data.ip === "string") {
    data.ip = fakeIP;
    body = JSON.stringify(data);
    var hits = Number($persistentStore.read("ipleak.hits") || "0") + 1;
    $persistentStore.write(String(hits), "ipleak.hits");
    // Log the count only, never the real address.
    console.log("ipleak-json: rewritten, hits=" + hits);
  }
} catch (error) {
  console.log("ipleak-json: body is not JSON, left unchanged");
}

$done({ body: body });
```

- `script-path` is an https URL pinned to a commit SHA;
- `requires-body=true` is needed to read the body, and `max-size` skips the
  script on unexpectedly large bodies;
- a non-JSON body is handed back unchanged without throwing;
- the log never prints the real address.

Offline replay (`cases.tsv`, tab-separated):

```text
case	json	response	https://ipleak.net/json/	{"country_code":"US","ip":"203.0.113.7"}
case	not-json	response	https://ipleak.net/json/	not json
```

```sh
build/anixops-mitm-runner replay --plugin ipleak-script.plugin --fixture cases.tsv \
  --script-runner e2e/script_runtime/anixops_runner.js \
  --script-map https://raw.githubusercontent.com/<you>/<repo>/<commit-sha>/ipleak-json.js=./ipleak-json.js \
  --script-store build/ipleak-store.json
```

Expected: the first body becomes
`{"country_code":"US","ip":"233.333.333.333"}`, the second stays `not json`.

## 10. Submission and contribution

### 10.1 Publishing your plugin

- Keep plugins and scripts in **your own repository** and distribute pinned
  https URLs (commit or release tag). This repository defines format and
  procedure only and does not carry service-specific plugins.
- Document what the plugin does, why each `[MITM]` host is needed, known
  gaps, how to turn it off or remove it, and your own license.
- To be registered in
  [`../recipes/external-examples.md`](../recipes/external-examples.md), meet
  its listing requirements and open a documentation pull request that adds
  one row to that page. A listing is an index entry, not a review or an
  endorsement.

### 10.2 Reporting compatibility problems

Open an issue with:

- a minimal reproducing plugin (hosts replaced by `*.example` / `*.test`, no
  credentials);
- the grade and reason of the relevant lines from the client's import report;
- the expected behavior and how it behaves in the original client.

### 10.3 Pull requests to this repository

1. Fork, branch, and keep one change per pull request.
2. For syntax or behavior changes, submit contract, fixtures, positive and
   negative tests, matrix row, and CHANGELOG as in
   [section 7](#7-fixtures-contracts-and-matrix-rows); record external manual
   steps in [`../manual-intervention.md`](../manual-intervention.md).
3. No new dependency unless a contract states why, alternatives, license
   impact, and how GitHub Actions verifies it.
4. GitHub Actions runs every check (format, build, tests, fixtures, matrix,
   security claims, ...). Green CI is required to merge.
5. Answer the "Compatibility Changes" questions in
   [`../../CONTRIBUTING.md`](../../CONTRIBUTING.md) in the description:
   ecosystem, contract, parser case, positive and negative test, matrix row.

### 10.4 Rejected in review

- The prohibited categories in [section 1](#1-read-first-terms-and-prohibited-uses):
  tampering with game settlement or values, evading anti-cheat, forging
  payments or orders, bypassing authentication, risk control, DRM, or
  certificate validation;
- a `[MITM]` scope larger than the plugin needs, such as `*` or broad
  wildcards;
- scripts that send cookies, tokens, accounts, or bodies to third parties, or
  log them;
- obfuscated or minified scripts that cannot be reviewed, or scripts loaded
  from a moving branch URL;
- asking to disable certificate validation, or to install or trust a
  certificate automatically.
