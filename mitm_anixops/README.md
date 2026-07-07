# mitm_anixops

Standalone C ABI library for an AnixOps-compatible MITM/rewrite subset.

This directory is intentionally independent from any upstream client repository. A client can link the
shared/static library and keep its own repository moving without carrying patch files.

The current library deliberately has no third-party dependency. TLS sockets, certificate storage, and platform
trust checks should be provided by a platform adapter or a later optional crypto feature.

## Scope

Implemented:

- AnixOps-style `[MITM] hostname = ...` parsing.
- Explicit MITM gate: enabled flag, trusted certificate state, allow/deny host patterns.
- QUIC decision output: return `ANIXOPS_MITM_REJECT_QUIC` when a MITM host is requested over QUIC and QUIC-for-MITM is disabled.
- AnixOps-style `[Rewrite]` URL rules for `302`, `307`, `reject`, `reject-200`, `reject-img`, `reject-video`, `reject-dict`, `reject-array`.
- Mock and regex request/response body rewrite application for already-buffered plain-text bodies.
- Request/response header rewrite dispatch for add, replace, delete, and regex replace actions.
- Regex capture replacement with `$1` and `\1`.
- AnixOps/Surge-style module script metadata for HTTP request/response hooks.
- AnixOps-style `[Argument]` defaults, Surge-style `#!arguments`, plus per-argument overrides for script `$argument` generation.
- BiliUniverse Enhanced-style `.plugin`, `.snippet`, and `.sgmodule` fixtures.
- C ABI result structs with no exposed internal pointers.

Not implemented yet:

- TLS socketing, dynamic leaf certificate generation, CA storage, or iOS trust detection.
- HTTP parser, HTTP/2 frame parser, compression/chunk handling, body buffering.
- JQ rewrite actions.
- JavaScript script runtime. The library returns script dispatch metadata; the client must run the JS.
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
that key C ABI symbols are exported when `nm` is available, runs the mihomo proxy-chain E2E when `MIHOMO_BIN` points to
an executable binary, runs the BiliUniverse script-runtime E2E fixture, and checks the generic request/response script
contract.

Test policy and layout are documented in `tests/README.md`.

For black-box proxy E2E with mihomo:

```sh
MIHOMO_BIN=/path/to/mihomo make e2e
```

The E2E fixture starts a local MITM/rewrite shim that calls this C ABI, runs mihomo as the outbound proxy core, and
uses `https://google.com:<local-port>` against a local HTTPS origin. `scripts/check.sh` runs this automatically when
the mihomo binary is present. See `e2e/README.md`.

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
anixops_engine_load_config(engine, config_text);
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

## Evidence

See `docs/anixops_mitm_evidence.md`.

For the BiliUniverse Enhanced fixture, see `docs/bili_enhanced_plugin_support.md`.

For the script runtime adapter boundary, see `docs/script_runtime_contract.md`.
