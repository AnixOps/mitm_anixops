# NetworkCore Integration Notes

This note records the expected integration shape for
`https://github.com/AnixOps/networkcore_anixops` as inspected on 2026-07-07 at
commit `3e6579c`.

## Compatibility Conclusion

`mitm_anixops` is compatible with `networkcore_anixops` as a portable MITM
policy/plugin C ABI core. It is not a complete cross-platform network engine.

The current `networkcore_anixops` repository is a Rust workspace with these
active members:

- `apps/linux-cli`
- `crates/config-core`
- `crates/control-domain`
- `crates/control-runtime`
- `crates/engine-native`
- `crates/platform-linux`

The best current insertion point is the domain MITM plugin boundary:

- `control_domain::MitmPluginService`
- `control_runtime::MitmGateOrchestrator`

That boundary can load plugin/config text, validate supported rule shapes, and
produce diagnostics or audit decisions. It is not yet enough for full traffic
rewrites because `control_domain::PluginResult` currently contains only audits
and diagnostics, not request/response mutation output.

## Current NetworkCore Gaps

The current `engine-native` data plane is still SOCKS-focused:

- runtime listeners support `LocalTcp` and `Socks`
- outbound runtime nodes support `Protocol::Socks`
- the accept loop reads SOCKS5 greeting, CONNECT target, outbound CONNECT
  response, and then relays TCP streams

It does not yet own:

- TLS MITM handshakes
- dynamic leaf certificate generation
- HTTP/1.1 parser
- HTTP/2 frame parser
- compression or chunked body handling
- request/response body buffering policy
- JavaScript execution

The current `platform-linux` crate is also a read-only capability adapter. It
reports MITM certificate status but does not install or trust certificates.
The iOS architecture document explicitly treats Network Extension, CA trust,
MITM scope, and remote script execution as high-risk platform work. macOS and
Windows platform crates are not present in the inspected workspace.

Therefore the accurate statement is:

- the `mitm_anixops` C ABI can be made available to NetworkCore on platforms
  with a C toolchain and Rust FFI support
- the complete product behavior still depends on NetworkCore platform adapters
  for proxy capture, TLS, certificates, HTTP framing, and script execution

## Recommended Repository Shape

Vendor or pin this repository into `networkcore_anixops` as one of:

- `third_party/mitm_anixops` git submodule
- `third_party/mitm_anixops` subtree
- a pinned source archive or release artifact downloaded by CI

For early development, a submodule is the simplest because it keeps ABI changes
reviewable.

Add two Rust crates to `networkcore_anixops`:

- `crates/mitm-anixops-sys`: unsafe C bindings and build/link logic
- `crates/mitm-policy`: safe Rust wrapper and domain adapter

Minimal `build.rs` shape for `crates/mitm-anixops-sys`:

```rust
fn main() {
    let root = "../../third_party/mitm_anixops";

    cc::Build::new()
        .file(format!("{root}/src/mitm_anixops.c"))
        .include(format!("{root}/include"))
        .define("ANIXOPS_STATIC", None)
        .warnings(true)
        .compile("mitm_anixops");

    println!("cargo:rerun-if-changed={root}/include/mitm_anixops.h");
    println!("cargo:rerun-if-changed={root}/src/mitm_anixops.c");
}
```

Use either `bindgen` or a small handwritten `extern "C"` module matching
`include/mitm_anixops.h`. Handwritten bindings are reasonable while the ABI is
small and guarded by `ci/abi_exports.txt`.

## Safe Wrapper Contract

The safe Rust wrapper should own the opaque C engine handle:

- `Engine::new() -> Result<Engine, Error>`
- `Engine::load_config(&mut self, text: &str) -> Result<(), Error>`
- `Engine::set_cert_state(&mut self, state: CertState)`
- `Engine::evaluate_mitm(&self, host: &str, is_quic: bool) -> MitmDecision`
- `Engine::evaluate_url_rewrite(&self, url: &str, phase: Phase) -> RewriteResult`
- `Engine::apply_body(&self, url: &str, phase: Phase, body: &[u8]) -> BodyRewriteResult`
- `Engine::evaluate_header_rewrite(...) -> HeaderRewriteResult`
- `Engine::evaluate_script(&self, url: &str, phase: Phase) -> ScriptDispatch`
- `Engine::last_error() -> Option<LastError>`

Do not mark `Engine` as `Sync`. The C engine is not internally locked. If
NetworkCore wants shared runtime use, put the wrapper behind a `Mutex` or use
immutable per-worker snapshots.

Map C status codes and `anixops_engine_copy_last_error` into
`control_domain::DomainError` and `control_domain::Diagnostic`.

## NetworkCore Integration Phases

Phase 1: adapter tests only.

- Implement a `MitmPluginService` adapter backed by `mitm-policy`.
- Load `PluginPackage.source` through `anixops_engine_load_config`.
- Validate supported plugin/rule syntax.
- Return diagnostics and audit events only.
- Add unit tests for allowed, denied, parse-error, and missing-permission paths.

Phase 2: domain mutation model.

- Extend NetworkCore domain types so plugin handling can return structured
  request/response mutations.
- Model URL redirect/reject, header add/replace/delete, body replacement, and
  script dispatch output.
- Keep JavaScript execution outside `mitm_anixops`; this library only returns
  script metadata.

Phase 3: native HTTP/TLS path.

- Add an HTTP/TLS MITM boundary to `engine-native`.
- On SNI or request host, call `evaluate_mitm`.
- Treat `ANIXOPS_MITM_REJECT_QUIC` as a signal to reject/drop QUIC for matched
  MITM hosts so clients can retry over TCP/TLS.
- After HTTP request parsing, call URL/header/request-script evaluation.
- After response headers/body buffering and decompression, call
  response-header, response-body, and response-script evaluation.

Phase 4: platform adapters.

- Linux: certificate install/trust checks and explicit user-controlled trust
  workflow.
- macOS: system trust integration and signed/notarized client packaging.
- Windows: user certificate store integration and signed binary packaging.
- iOS: Network Extension, embedded runtime, explicit CA trust detection, and a
  conservative script policy that follows App Review constraints.

## CI Requirements

NetworkCore should prove integration in GitHub Actions before claiming platform
support:

- Linux Rust build/test with the C static library compiled by `cc`
- macOS Rust build/test with the C static library compiled by `cc`
- Windows Rust build/test with the C static library compiled by `cc`
- ABI allowlist comparison against `mitm_anixops/ci/abi_exports.txt`
- wrapper tests for config diagnostics, MITM decisions, URL rewrites, header
  rewrites, body rewrites, and script dispatch
- optional iOS `cargo check` or Xcode validation on a macOS runner after an iOS
  platform crate exists

Until those jobs pass, call the integration "planned" or "adapter-compatible",
not "fully cross-platform available".
