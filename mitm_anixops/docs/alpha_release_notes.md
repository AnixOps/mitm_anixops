# Alpha Release Notes

## Scope

This Alpha package is a no-UI, embeddable MITM policy/runtime foundation. It is not a full GUI client and does not
claim complete LOON parity.

Included:

- `libmitm_anixops.a` and `libmitm_anixops.so`
- public C ABI header
- relocatable `lib/pkgconfig/mitm_anixops.pc`
- relocatable CMake package config under `lib/cmake/mitm_anixops`
- Go cgo binding package under `bindings/go/anixops`
- Rust FFI wrapper crate under `bindings/rust/mitm-anixops`
- `anixops-mitm-runner` with `scan`, `trace`, and no-network `replay` commands
- `anixops-mitm-proxy-shim` Alpha/demo proxy shim
- `anixops-script-runner.js` Node-based script contract runner for demos and E2E replay
- representative runner and script-runtime replay fixtures
- compatibility and roadmap documentation

## Implemented Toward P1-P6

- P1 parser foundation: compatibility profile selector, per-rule accepted/ignored/rejected diagnostics, `[Plugin]`
  metadata tolerance, `[Body Rewrite]` and `[Remote Body Rewrite]` aliases.
- P2 regex foundation: regex backend selector with POSIX Lite default and optional PCRE2 compile/match/replace support
  when built with `PCRE2=1`; NSRegularExpression is still a future Darwin backend.
- P3 JQ foundation: `request-body-jq`, `http-request-jq`, `response-body-jq`, and `http-response-jq` rules parse and
  match. Default builds fail open with `jq backend unavailable`; `JQ=1` builds execute through libjq with tested
  first-output, empty-output, compile-error, and invalid-JSON behavior.
- P4 script foundation: script dispatch metadata, the Node-based contract runner, file-backed `$persistentStore`,
  request/response script E2E, script timeout fail-open in the proxy shim, and runner `replay --script-runner`
  writeback are supported. Embedded QuickJS/JavaScriptCore is still future work.
- P5 runner foundation: `scan`, `scan --corpus`, `trace`, and TSV-backed `replay` commands provide no-UI diagnostics,
  corpus count, sha256, and diagnostic-status checks, and comparable URL, rewrite, header, body, and script traces;
  `anixops_rewrite_build_plan`
  provides a C ABI foundation that aggregates phase rewrite, header rewrites, script dispatch, body-rewrite output, and
  `requires_body`; the Alpha proxy shim provides HTTP/1.1 CONNECT/TLS demo coverage around the C ABI, including buffered
  request/response header and body rewrite before script dispatch and gzip/deflate response decoding for body/script
  mutation with identity writeback.
- P6 packaging foundation: `make alpha-dist` creates a tar.gz package with libraries, pkg-config metadata, CMake package
  config, Go cgo binding, Rust FFI wrapper, runner, proxy shim, script runner, representative fixtures, the corpus
  manifest, header, and docs. The Go and Rust Alpha wrappers expose the aggregated rewrite plan in addition to
  rewrite/body/script helpers.

## Known Gaps

- No embedded QuickJS or JavaScriptCore runtime yet.
- `$persistentStore` has an Alpha file-backed Node runner implementation; production namespacing, locking, transactions,
  and platform storage backends remain future work.
- Script timeout/error fail-open is covered in the Alpha proxy shim; production script scheduling, cancellation, memory
  limits, and cache policy remain future work.
- Optional libjq execution exists, but resource limits, cache/reuse policy, and broad jq corpus coverage are not complete.
- No NSRegularExpression Darwin backend yet.
- The Rust and Go bindings are Alpha wrappers, not versioned registry/module releases yet. The CMake package config is
  included and covered by a staged configure/build/run smoke in the Alpha verification environment.
- The packaged proxy shim is Alpha/demo quality and HTTP/1.1 oriented; buffered header/body rewrite ordering and
  gzip/deflate scripted response bodies are covered, but no production HTTP/2, brotli/zstd, streaming body, or platform
  capture adapter is included.
- No iOS/macOS Network Extension adapter yet.

## Verification

Run:

```sh
sh scripts/check.sh
make alpha-dist
```

The check gate covers unit tests, the C ABI plan builder, optional PCRE2/libjq builds when headers are present,
pkg-config compile/run smoke, CMake package configure/build/run smoke, Go cgo binding tests, Rust wrapper tests, ABI
exports, strategy demo, runner corpus scan with sha256 and diagnostic-status checks, runner replay with and without
script execution, proxy-shim smoke tests, mihomo proxy E2E, BiliUniverse fixture, generic script contract E2E with
persistentStore, header/body rewrite ordering, gzip/deflate response decode coverage, and script timeout fail-open,
plus the Bilibili homepage demo E2E. Go and Rust wrapper tests also cover the aggregated plan helper.
