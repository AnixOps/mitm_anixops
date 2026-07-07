# mitm_anixops

Standalone C ABI library for an AnixOps-compatible MITM/rewrite subset.

This directory is intentionally independent from any upstream client repository. A client can link the
shared/static library and keep its own repository moving without carrying patch files.

The current library deliberately has no third-party dependency. TLS sockets, certificate storage, and platform
trust checks should be provided by a platform adapter or a later optional crypto feature.

## Scope

The tested compatibility surface is tracked in `docs/compatibility_matrix.md`. Remaining gaps and explicit non-goals
are tracked in `docs/TODO.md`.

Implemented:

- AnixOps-style `[MITM] hostname = ...` parsing.
- Explicit MITM gate: enabled flag, trusted certificate state, allow/deny host patterns.
- QUIC decision output: return `ANIXOPS_MITM_REJECT_QUIC` when a MITM host is requested over QUIC and QUIC-for-MITM is disabled.
- AnixOps-style `[Rewrite]` URL rules for `302`, `307`, `reject`, `reject-200`, `reject-401`, `reject-img`,
  `reject-video`, `reject-dict`, `reject-array`.
- Mock and regex request/response body rewrite application for already-buffered plain-text bodies.
- JSON path request/response body replacement for already-buffered JSON bodies, using object paths, bracket string keys,
  and array indexes such as `$.enabled`, `$['profile.meta'].name`, and `$.items[0].title`, with raw JSON literals such
  as `true`, `null`, objects, arrays, or quoted strings.
- Request/response header rewrite dispatch for add, replace, delete, and regex replace actions.
- Regex capture replacement with `$1` and `\1`.
- POSIX ERE regex matching with a supported leading `(?i)` case-insensitive prefix and PCRE shorthand classes
  `\d`, `\w`, and `\s`, plus PCRE absolute anchors `\A`, `\z`, and `\Z`.
- PCRE non-capturing group syntax `(?:...)` for matching, normalized as a regular POSIX capturing group.
- AnixOps/Surge-style module script metadata for HTTP request/response hooks.
- AnixOps-style `[Argument]` defaults, Surge-style `#!arguments`, plus per-argument overrides for script `$argument` generation.
- BiliUniverse Enhanced-style `.plugin`, `.snippet`, and `.sgmodule` fixtures.
- Status text and last-error diagnostics for config/add-rule failures, including config line numbers.
- C ABI result structs with no exposed internal pointers.

Not implemented yet:

- TLS socketing, dynamic leaf certificate generation, CA storage, or iOS trust detection.
- HTTP parser, HTTP/2 frame parser, compression/chunk handling, body buffering.
- Full JQ rewrite actions beyond the tested JSON path replacement subset.
- JavaScript script runtime. The library returns script dispatch metadata; the client must run the JS.
- Full NSRegularExpression/PCRE syntax beyond POSIX ERE plus the tested leading `(?i)` prefix, shorthand classes,
  absolute anchors, and non-capturing group matching subset.
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

The check script cleans the build, compiles the static and shared libraries, runs the separated test fixture, verifies
the complete exported C ABI symbol allowlist in `ci/abi_exports.txt` when `nm` is available, runs the minimal
strategy-chain demo, runs the mihomo proxy-chain E2E when `MIHOMO_BIN` points to an executable binary, runs the
BiliUniverse script-runtime E2E fixture, and checks the generic request/response script contract. GitHub Actions runs
the same entrypoint after installing the pinned mihomo fixture with `scripts/ensure-mihomo.sh`.

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

anixops_header_rewrite_result_t header;
anixops_rewrite_evaluate_header(engine, "https://api.example.com/mock", ANIXOPS_PHASE_RESPONSE, 0, "old", &header);

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
- Enumerate matched header rewrite records with `anixops_rewrite_evaluate_header`, then apply them to the platform HTTP
  header map.
- Feed matched `anixops_script_result_t` records into the platform JavaScript runtime with `$request`, `$response`,
  `$argument`, `$persistentStore`, and `$done` bindings.
- Treat `ANIXOPS_MITM_REJECT_QUIC` as a signal to reject/drop QUIC for a matched host so the client can retry over TCP/TLS.
- Follow `docs/script_runtime_contract.md` for request/response script globals, `$done` writeback, timeout behavior, and
  body-framing ownership.

For Rust:

- Generate bindings from `include/mitm_anixops.h`, or write a thin `extern "C"` wrapper.
- Keep this directory as a submodule, vendored dependency, or prebuilt artifact.
- For the inspected `networkcore_anixops` Rust workspace integration path, see
  `docs/networkcore_integration.md`.

## Evidence

See `docs/anixops_mitm_evidence.md`.

For the supported configuration grammar and public-API evidence, see `docs/compatibility_matrix.md`.

For the BiliUniverse Enhanced fixture, see `docs/bili_enhanced_plugin_support.md`.

For the script runtime adapter boundary, see `docs/script_runtime_contract.md`.

For `networkcore_anixops` compatibility and integration staging, see `docs/networkcore_integration.md`.

For release blockers, compatibility gaps, and stage non-goals, see `docs/TODO.md`.

For publishing source or binary artifacts, see `docs/release_checklist.md`.
