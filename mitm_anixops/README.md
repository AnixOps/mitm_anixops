# mitm_anixops

Standalone C ABI library for an AnixOps-compatible MITM/rewrite subset.

This directory is intentionally independent from any upstream client repository. A client can link the
shared/static library and keep its own repository moving without carrying patch files.

The default C library build deliberately has no third-party dependency. Optional PCRE2/libjq builds and the Alpha proxy
shim add platform dependencies without changing the default policy-core ABI. TLS sockets, certificate storage, and
platform trust checks should be provided by a platform adapter or an explicit runner/shim layer.

## Scope

The tested compatibility surface is tracked in `docs/compatibility_matrix.md`. Remaining gaps and explicit non-goals
are tracked in `docs/TODO.md`.

The v1.0.0 maintenance track is governed by `ROADMAP.md`, `TODO.md`, `CHANGELOG.md`, `CONTRIBUTING.md`,
`docs/architecture/`, `docs/compatibility/`, and `docs/manual-intervention.md`. That track follows a CI/CD-only
acceptance model: local machines are used for source and documentation edits, while build, test, package, release
dry-run, and release publication evidence must come from GitHub Actions.

The full LOON MITM plugin compatibility and no-UI runtime plan is tracked in
`docs/loon_mitm_full_compat_plan.md`, with a shorter Chinese presentation draft in
`docs/loon_mitm_full_compat_brief_zh.md`.

Implemented:

- AnixOps-style `[MITM] hostname = ...` parsing plus Quantumult X `force-http-engine-hosts` host-list parsing.
- Explicit MITM gate: enabled flag, trusted certificate state, allow/deny host patterns, and adapter-readable
  `skip-server-cert-verify` boolean or host-list config.
- QUIC decision output: return `ANIXOPS_MITM_REJECT_QUIC` when a MITM host is requested over QUIC and QUIC-for-MITM is disabled.
- AnixOps-style `[Rewrite]` URL rules for standard redirects `301`, `302`, `303`, `307`, `308`, plus `reject`,
  `reject-200`, `reject-401`, `reject-img`, `reject-tinygif`, `reject-video`, `reject-dict`, `reject-array`, and
  numeric `reject-NNN` statuses.
- Rewrite section aliases including `[URL Rewrite]`, `[Remote Rewrite]`, `[Header Rewrite]`, and
  `[Remote Header Rewrite]`.
- Mock and regex request/response body rewrite application for already-buffered plain-text bodies.
- JSON path request/response body replacement for already-buffered JSON bodies, using object paths, bracket string keys,
  empty, common escaped, and `\uXXXX` bracket string keys, and array indexes such as `$.enabled`,
  `$['profile.meta'].name`, `$.items[0].title`, and `$.items[-1].title`, with raw JSON literals such as `true`, `null`,
  objects, arrays, or quoted strings.
- Request/response header rewrite dispatch for add, replace, delete, and regex replace actions, including
  case-insensitive named-header lookup and bounded header-list application for multi-value add/replace/delete and
  independent `Set-Cookie` fields.
- Quantumult X `url`-prefixed rewrite forms for redirect, reject, body rewrite, JSON body rewrite, header rewrite,
  `echo-response`, and request/response script actions.
- Regex capture replacement with `$1`, `${1}`, `\1`, `${name}`, and `$<name>`.
- POSIX ERE regex matching with supported leading `(?i)`, `(?m)`, `(?s)`, and combined flag prefixes, PCRE shorthand classes
  `\d`, `\D`, `\w`, `\W`, `\s`, `\S`, `\h`, `\H`, `\v`, and `\V`, PCRE control escapes such as `\t`, PCRE hex
  byte escapes such as `\x2e`, PCRE Unicode escapes such as `\u00e9`, lazy quantifier suffixes normalized to greedy
  equivalents, plus PCRE absolute anchors `\A`, `\z`, and `\Z`.
- PCRE non-capturing group syntax `(?:...)` for matching without consuming replacement capture indexes.
- PCRE named capture group syntax `(?<name>...)` and `(?'name'...)` for matching and replacement.
- PCRE quoted literal syntax `\Q...\E`, normalized as escaped POSIX ERE literal text.
- Stable global regex replacement behavior for empty matches such as `^`, `$`, and lazy `.*?` normalization in body and
  header rewrite paths.
- AnixOps/Surge-style module script metadata for HTTP request/response hooks, including `enable` dispatch gating and
  rule-level `timeout` / `max-size` scheduling metadata.
- AnixOps-style `[Argument]` defaults, Surge-style `#!arguments`, tolerated `#!` metadata diagnostics, plus
  per-argument overrides for script `$argument` generation.
- Optional libjq execution for `request-body-jq`, `http-request-jq`, `response-body-jq`, and `http-response-jq` when built
  with `JQ=1`, including a configurable max-input-bytes fail-open guard; the default no-libjq build remains fail-open.
- BiliUniverse Enhanced-style `.plugin`, `.snippet`, and `.sgmodule` fixtures.
- Status text and last-error diagnostics for config/add-rule failures, including config line numbers.
- C ABI result structs with no exposed internal pointers.
- C ABI plan builder, `anixops_rewrite_build_plan`, that aggregates the phase rewrite, matching header rewrites, script
  dispatch, body-rewrite output, `requires_body`, and script scheduling metadata for adapter pipelines.

Not implemented yet:

- TLS socketing, dynamic leaf certificate generation, CA storage, or iOS trust detection.
- HTTP parser, HTTP/2 frame parser, compression/chunk handling, body buffering.
- Full JQ compatibility beyond the optional libjq first-output execution policy, including timeout/memory limits and
  broader corpus coverage.
- JavaScript script runtime. The library returns script dispatch metadata; the client must run the JS.
- Full NSRegularExpression/PCRE syntax beyond POSIX ERE plus the tested leading `(?i)`/`(?m)`/`(?s)` prefixes,
  shorthand classes, absolute anchors, named capture groups, quoted literal matching, and empty-match replacement
  subset.
- Full AnixOps compatibility.

## Build

```sh
make
make test
```

Outputs:

```text
build/libmitm_anixops.so
build/libmitm_anixops.a
```

For the no-UI Alpha runner:

```sh
make runner
build/anixops-mitm-runner scan tests/fixtures/Representative.Loon.plugin
build/anixops-mitm-runner scan --corpus tests/fixtures/corpus/manifest.json
build/anixops-mitm-runner trace --plugin tests/fixtures/Representative.Loon.plugin --url https://api.loon.example/v1 --phase response
build/anixops-mitm-runner replay --plugin tests/fixtures/Representative.Loon.plugin --fixture tests/fixtures/RunnerReplay.tsv
build/anixops-mitm-runner replay --plugin tests/fixtures/Representative.Loon.plugin --fixture tests/fixtures/RunnerReplay.tsv \
  --script-runner e2e/script_runtime/anixops_runner.js \
  --script-store build/runner-script-store.json \
  --script-map https://scripts.example/loon-request.js=tests/fixtures/runner_replay_script.js \
  --script-map https://scripts.example/loon-response.js=tests/fixtures/runner_replay_script.js
build/anixops-mitm-runner replay --plugin tests/fixtures/Representative.Loon.plugin --fixture tests/fixtures/RunnerReplay.tsv \
  --script-runner e2e/script_runtime/anixops_runner.js \
  --script-bundle tests/fixtures/ScriptBundle.json
```

`scan --corpus` reads `tests/fixtures/corpus/manifest.json`, resolves fixture paths relative to that manifest, verifies
fixture sha256 digests, and emits a stable JSON report with source metadata, expected parser status, expected rule
counts, observed counts, accepted/ignored/rejected diagnostic counts, and an overall `passed` flag. `replay` supports
explicit `--script-map` assets or an offline `--script-bundle` manifest with per-script sha256 checks, and uses a stable
TSV fixture format for no-network trace checks:

```text
case<TAB>name<TAB>request|response<TAB>url<TAB>body-or-
```

For the Alpha HTTP/TLS proxy shim:

```sh
make proxy-shim
build/anixops-mitm-proxy-shim --bilibili-demo --listen 127.0.0.1:19080 --upstream http://127.0.0.1:7890 \
  --script-store build/anixops-script-store.json --script-timeout-ms 5000 --debug
```

The proxy shim is a demo/Alpha integration artifact around the C ABI. It supports HTTP/1.1 CONNECT/TLS MITM, generated
test CA material, upstream proxy forwarding, URL rewrite, buffered request/response header and body rewrite, script
runner integration with file-backed `$persistentStore`, and gzip/deflate response decoding for body/script mutation
with identity writeback. Script runner timeout/errors fail open after static rewrites. It is not a production HTTP/2,
brotli/zstd, or streaming body pipeline.

For a local Alpha package:

```sh
make alpha-dist
```

That writes `build/anixops-mitm-alpha-0.45.10.tar.gz`. Alpha scope and known gaps are documented in
`docs/alpha_release_notes.md`. The package includes representative Loon, Surge, Quantumult X, and BiliBili fixtures,
`fixtures/corpus/manifest.json`, and `fixtures/RunnerReplay.tsv` so the runner can be exercised without the source
tree; it also includes `fixtures/runner_replay_script.js` and script bundle fixtures for runtime replay,
`lib/pkgconfig/mitm_anixops.pc` for C consumers, and `lib/cmake/mitm_anixops/mitm_anixops-config.cmake` for CMake
consumers.

For pkg-config integration:

```sh
make pkg-config-check
PKG_CONFIG_PATH=/path/to/anixops-mitm-alpha-0.45.10/lib/pkgconfig pkg-config --cflags --libs mitm_anixops
```

For CMake package metadata coverage:

```sh
make cmake-package-check
```

Consumers with CMake can use `find_package(mitm_anixops CONFIG REQUIRED)` and link `mitm_anixops::mitm_anixops`.

For Go cgo binding coverage:

```sh
make go-binding-check
```

The Go package lives under `bindings/go/anixops` and uses `pkg-config: mitm_anixops`, so consumers should point
`PKG_CONFIG_PATH` at the Alpha package's `lib/pkgconfig` directory. The Alpha wrapper exposes config load, rewrite/body
evaluation, body-chain application, header-list application, script dispatch, JQ max-input configuration, and the
aggregated rewrite plan.

For Rust binding coverage:

```sh
make rust-binding-check
```

The Rust crate lives under `bindings/rust/mitm-anixops` and uses `pkg-config` from `build.rs`, so consumers should point
`PKG_CONFIG_PATH` at the Alpha package's `lib/pkgconfig` directory and set `LD_LIBRARY_PATH` to the package `lib`
directory when running tests against the shared library. The Alpha wrapper exposes rewrite/body evaluation,
body-chain application, named-header lookup, bounded header-list application, script dispatch, JQ max-input
configuration, and the aggregated rewrite plan.

For optional PCRE2 regex backend coverage:

```sh
make pcre2-test
make PCRE2=1 all runner
```

For optional libjq body rewrite execution:

```sh
make jq-test
make JQ=1 all runner
```

## Windows Binary

GitHub Actions builds a no-UI Windows x64 zip artifact named `anixops-mitm-windows-x64`.

For the built-in Bilibili homepage MITM demo:

```powershell
.\build\anixops-mitm.exe --bilibili-demo --listen 127.0.0.1:19080 --upstream http://127.0.0.1:7890 --install-ca --debug
```

Then set the browser HTTP and HTTPS proxy to `127.0.0.1:19080` and open `https://www.bilibili.com/`.

For a Shadowsocks link through mihomo:

```powershell
.\build\anixops-mitm.exe --bilibili-demo --upstream "ss://BASE64_METHOD_PASSWORD@host:port#name" --core-bin C:\path\to\mihomo.exe --install-ca --debug
```

## Required Check

Run this before every commit:

```sh
sh scripts/check.sh
```

The check script cleans the build, compiles the static and shared libraries, runs the separated test fixture, runs
optional PCRE2/libjq test builds when headers are present, verifies the complete exported C ABI symbol allowlist in
`ci/abi_exports.txt` when `nm` is available, runs the minimal strategy-chain demo, runs the no-UI runner corpus scan and
replay with and without script execution, runs the proxy-shim smoke check, runs the mihomo proxy-chain E2E when
`MIHOMO_BIN` points to an executable binary, runs the BiliUniverse script-runtime E2E fixture, and checks the generic
request/response script contract including `$persistentStore`, rule-level script timeout fail-open, static header/body
rewrite ordering, and gzip/deflate response decode coverage. GitHub Actions runs the same entrypoint after installing the
pinned mihomo fixture with `scripts/ensure-mihomo.sh`.

Test policy and layout are documented in `tests/README.md`.

For black-box proxy E2E with mihomo:

```sh
MIHOMO_BIN=/path/to/mihomo make e2e
```

The E2E fixture starts a local MITM/rewrite shim that calls this C ABI, runs mihomo as the outbound proxy core, and
uses `https://google.com:<local-port>` against a local HTTPS origin. `scripts/check.sh` runs this automatically when
the mihomo binary is present. See `e2e/README.md`.

For the minimal pure-C strategy-chain demo:

```sh
make demo-check
```

That fixture builds `examples/strategy_chain_demo.c`, loads an embedded config, then prints and asserts the complete
library-only flow: config load, MITM allow/deny/QUIC decisions, URL rewrite, request/response header rewrite,
response body rewrite, and response script dispatch metadata. It intentionally does not open sockets, parse HTTP, run
TLS, or execute JavaScript.

For BiliUniverse Enhanced-style plugin script dispatch and execution:

```sh
make bili-e2e
```

That fixture loads the real `BiliBili.Enhanced.plugin`, resolves the matched `response.bundle.js` hook through this C
ABI, verifies the pinned `.snippet` and `.sgmodule` release assets dispatch to the same hook shape, then runs the real
bundle in a Node.js AnixOps-compatible script environment.

For the generic script request/response contract:

```sh
make script-contract-e2e
```

## C API Sketch

```c
anixops_engine_t *engine = anixops_engine_new();
int rc = anixops_engine_load_config(engine, config_text);
if (rc != ANIXOPS_OK) {
    int status;
    size_t line;
    char message[ANIXOPS_MESSAGE_CAP];
    anixops_engine_copy_last_error(engine, &status, &line, message, sizeof(message));
    /* log status, line, message */
}
anixops_engine_set_mitm_enabled(engine, 1);
anixops_engine_set_cert_state(engine, ANIXOPS_CERT_TRUSTED);

anixops_mitm_decision_t mitm;
anixops_mitm_evaluate(engine, "api.example.com", 0, &mitm);

anixops_rewrite_result_t rewrite;
anixops_rewrite_evaluate_url(engine, "http://google.cn", ANIXOPS_PHASE_REQUEST, &rewrite);

char body[4096];
anixops_rewrite_apply_body(engine, "https://api.example.com/mock", ANIXOPS_PHASE_RESPONSE, "{}", body, sizeof(body), &rewrite);

anixops_body_rewrite_chain_t body_chain;
anixops_rewrite_apply_body_chain(
    engine,
    "https://api.example.com/mock",
    ANIXOPS_PHASE_RESPONSE,
    "{}",
    body,
    sizeof(body),
    &body_chain);

anixops_header_rewrite_result_t header;
anixops_rewrite_evaluate_header(engine, "https://api.example.com/mock", ANIXOPS_PHASE_RESPONSE, 0, "old", &header);

anixops_header_list_t headers = {0};
anixops_header_list_t rewritten_headers = {0};
anixops_rewrite_plan_t header_plan;
anixops_rewrite_apply_headers(
    engine,
    "https://api.example.com/mock",
    ANIXOPS_PHASE_RESPONSE,
    &headers,
    &rewritten_headers,
    &header_plan);

anixops_script_result_t script;
anixops_script_evaluate_url(engine, "https://app.bilibili.com/x/resource/show/tab/v2?build=1", ANIXOPS_PHASE_RESPONSE, &script);

anixops_engine_free(engine);
```

`anixops_status_message(status)` returns stable static text for status codes. `anixops_engine_copy_last_error` copies
the most recent mutating API diagnostic into caller-owned storage; it never returns pointers into the engine. For
`anixops_engine_load_config`, `out_line` is the 1-based source line that failed, or `0` when the failure is not tied to
a config line.

## Threading

An engine is not internally locked. Treat each `anixops_engine_t` as single-writer and externally synchronized:

- Build or mutate an engine on one thread at a time.
- Concurrent read-only evaluation is allowed only when no thread is mutating or clearing the same engine.
- Use separate engine instances, or an adapter-owned lock/snapshot scheme, when integrating with multi-threaded network
  stacks.

## Integration Shape

For iOS:

- Link this library into the Network Extension target.
- Keep `NEPacketTunnelProvider`, TUN/tcp proxy, TLS, and certificate trust detection in the platform adapter.
- Feed HTTP request/response metadata into this library for policy decisions.
- Use `anixops_rewrite_build_plan` to aggregate URL/body rewrite, matching header rewrite records, script dispatch, and
  the `requires_body` signal for a request or response phase. Adapters that need per-header current values can still
  enumerate matched header rewrite records directly with `anixops_rewrite_evaluate_header`.
- Feed matched `anixops_script_result_t` records into the platform JavaScript runtime with `$request`, `$response`,
  `$argument`, `$persistentStore`, `$done`, and the returned `timeout_ms` / `max_size` scheduling metadata.
- Treat `ANIXOPS_MITM_REJECT_QUIC` as a signal to reject/drop QUIC for a matched host so the client can retry over TCP/TLS.
- Follow `docs/script_runtime_contract.md` for request/response script globals, `$done` writeback, timeout behavior, and
  body-framing ownership. The packaged Node contract runner supports file-backed `$persistentStore` through `--store`.

For Rust:

- Use the Alpha wrapper in `bindings/rust/mitm-anixops`, which exposes config load, rewrite/body evaluation, script
  dispatch, and the aggregated rewrite plan. Generate additional bindings from `include/mitm_anixops.h` for APIs that
  wrapper does not expose yet.
- Keep this directory as a submodule, vendored dependency, or prebuilt artifact.
- For the current `networkcore_anixops` Rust workspace integration path, see
  `docs/networkcore_integration.md`.

## Evidence

See `docs/anixops_mitm_evidence.md`.

For the supported configuration grammar and public-API evidence, see `docs/compatibility_matrix.md`.

For the BiliUniverse Enhanced fixture, see `docs/bili_enhanced_plugin_support.md`.

For the script runtime adapter boundary, see `docs/script_runtime_contract.md`.

For the v1.0.0 script runtime dependency decision, see
`docs/architecture/script-runtime-dependency.md`.

For the certificate lifecycle and MITM trust boundary, see
`docs/architecture/certificate-lifecycle.md`.

For `networkcore_anixops` compatibility and integration staging, see `docs/networkcore_integration.md`.

For release blockers, compatibility gaps, and stage non-goals, see `docs/TODO.md`.

For publishing source or binary artifacts, see `docs/release_checklist.md`.
