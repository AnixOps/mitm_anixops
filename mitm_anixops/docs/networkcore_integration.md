# NetworkCore Integration Notes

This note records the integration shape for
`https://github.com/AnixOps/networkcore_anixops`.

Inspection date: 2026-07-08.

Inspected upstream `main`:
`6e2f0a6080e00cf4a089367f34bb83729790e0a9`.

Current `mitm_anixops` alpha baseline:
`v0.45.10-alpha`
(`a3ee0fca6376ddccc333bdfe06ac5b5e75ed23e0`).

```text
networkcore-integration-alpha-baseline=v0.45.10-alpha
networkcore-integration-upstream-main=6e2f0a6080e00cf4a089367f34bb83729790e0a9
networkcore-integration-current-mode=vendored-policy-core-adapter
networkcore-integration-v1-boundary=adapter-owned-data-plane
networkcore-integration-live-mutation-status=deferred
networkcore-integration-ci-source-of-truth=github-actions
integration-adapter-readiness-gate=scripts/integration-adapter-readiness-check.sh
integration-adapter-readiness-self-test=scripts/integration-adapter-readiness-check-test.sh
integration-adapter-readiness-status=ci-gated-alpha-boundary
integration-adapter-readiness-release-artifact=policy-core-runner-proxy-shim-bindings-docs
integration-adapter-readiness-release-manifest=adapter-readiness-fields-required
integration-adapter-readiness-production-claim=not-production-networkcore-adapter
```

## Compatibility Conclusion

`mitm_anixops` is compatible with `networkcore_anixops` as a portable MITM
policy/plugin C ABI core. It is not a complete cross-platform network engine.

The inspected NetworkCore repository has moved past the original design-only
state. It now includes:

- `third_party/mitm_anixops`, pinned to the `v0.45.10-alpha` source baseline;
- `crates/mitm-anixops-sys`, the unsafe Rust FFI crate that compiles the C core;
- `crates/mitm-policy`, the safe Rust wrapper and NetworkCore policy adapter;
- `docs/architecture/mitm-anixops-adapter.md`, the upstream adapter design;
- `MitmPluginService` integration that returns audit/diagnostics while live
  request/response mutation remains deferred.

The accurate current statement is:

- NetworkCore can embed the `v0.45.10-alpha` policy core through a vendored C
  ABI and Rust safe wrapper.
- NetworkCore can use the wrapper for config diagnostics, MITM decisions, URL
  rewrite plans, header/body/script policy outputs, and deferred mutation
  diagnostics.
- NetworkCore still owns the production data plane: socket IO, TLS, certificate
  lifecycle, HTTP parsing/framing, compression, JavaScript runtime, storage,
  platform permissions, and release gates.

## Current Upstream Boundary

The current NetworkCore workspace includes these integration-relevant members:

- `apps/linux-cli`
- `apps/ios`
- `crates/config-core`
- `crates/control-domain`
- `crates/control-runtime`
- `crates/engine-native`
- `crates/engine-singbox`
- `crates/mitm-anixops-sys`
- `crates/mitm-policy`
- `crates/platform-ios`
- `crates/platform-linux`

The active policy insertion point remains:

- `control_domain::MitmPluginService`
- `control_runtime::MitmGateOrchestrator`
- `mitm_policy::AnixOpsMitmPluginService`

That boundary can load plugin/config text, validate supported rule shapes, map
C diagnostics into NetworkCore diagnostics, and expose safe wrapper outputs for
contract tests. It is still not the production traffic rewrite path.

## Adapter-Owned Responsibilities

For v1.0.0, `mitm_anixops` owns policy-core behavior only:

- parse and normalize supported config/plugin syntax;
- evaluate MITM hostname policy;
- evaluate URL, header, body, and script-trigger rules;
- produce structured rewrite plans, script dispatch metadata, and diagnostics;
- keep the C ABI and wrapper-facing behavior stable.

NetworkCore or another host adapter owns:

- socket IO and upstream routing;
- SNI handling and TLS handshakes;
- root CA generation, storage, install, trust detection, revoke, and rollback;
- dynamic leaf certificate generation and signing material protection;
- HTTP/1.1 parsing and serialization;
- HTTP/2 framing and stream handling;
- QUIC behavior;
- body buffering, decompression, recompression, chunking, and size limits;
- production JavaScript runtime execution and scheduling;
- `$persistentStore` storage, quotas, locking, and persistence;
- platform prompts, permissions, app-store policy, and enterprise policy;
- user-facing CLI/app surfaces for enabling or disabling MITM.

This is the future v1 boundary:

```text
mitm_anixops = policy-core + C ABI + diagnostics + mutation plans
NetworkCore = adapter + data plane + platform trust + runtime execution
```

## Current NetworkCore Gaps

The upstream `crates/mitm-policy` README and adapter design currently state that
user-facing MITM is not available yet. The current Linux release does not expose
a `networkcore-linux mitm` command, does not generate or install a CA, does not
decrypt HTTPS traffic, and does not apply rewrite plans to live traffic.

The remaining gates are:

- `MITM_CLI_COMMAND_GATE`: add a user-visible command surface with stable
  status/diagnostic output.
- `MITM_CERTIFICATE_LIFECYCLE_GATE`: implement CA generation, install, trust
  detection, revocation, and rollback boundaries.
- `MITM_HTTP_TLS_DATA_PLANE_GATE`: wire HTTP/TLS interception to
  `mitm-policy` rewrite plans.

These gates must pass in GitHub Actions before either repository claims
production NetworkCore MITM support.

## Integration Adapter Readiness Gate

For v1.2.0, `scripts/integration-adapter-readiness-check.sh` ties the
NetworkCore boundary markers, alpha release artifact contents, runner/proxy shim
targets, binding/package smoke checks, local full-check entrypoint, and
GitHub Actions release gates into one CI-verifiable adapter readiness contract.
`scripts/integration-adapter-readiness-check-test.sh` proves that the gate fails
when NetworkCore boundary evidence, proxy shim packaging evidence, or
production adapter claim boundaries are missing.

For v1.2.1, release and release-dry-run manifests, notes, and summaries must
carry the same adapter readiness status, gate, scope, and production-boundary
evidence before metadata validation passes.

The gate means `mitm_anixops` is packaged and CI-gated as a policy-core adapter
surface for NetworkCore-style hosts. It does not mean live NetworkCore HTTP/TLS
mutation, certificate lifecycle, platform trust, scheduler/runtime execution,
or production data-plane behavior is implemented by this repository.

## Safe Wrapper Contract

The NetworkCore safe wrapper should continue to own the opaque C engine handle:

- `Engine::new() -> Result<Engine, Error>`
- `Engine::load_config(&mut self, text: &str) -> Result<(), Error>`
- `Engine::set_cert_state(&mut self, state: CertState)`
- `Engine::evaluate_mitm(&self, host: &str, is_quic: bool) -> MitmDecision`
- `Engine::evaluate_url_rewrite(&self, url: &str, phase: Phase) -> RewriteResult`
- `Engine::apply_body_chain(&self, url: &str, phase: Phase, body: &[u8]) -> BodyRewriteChain`
- `Engine::evaluate_header_rewrite(...) -> HeaderRewriteResult`
- `Engine::evaluate_script(&self, url: &str, phase: Phase) -> ScriptDispatch`
- `Engine::build_plan(...) -> RewritePlan`
- `Engine::last_error() -> Option<LastError>`

Do not mark `Engine` as `Sync`. The C engine is not internally locked. If
NetworkCore wants shared runtime use, put the wrapper behind a `Mutex`, use
per-worker engines, or publish immutable per-worker snapshots.

Map C status codes and `anixops_engine_copy_last_error` into
`control_domain::DomainError` and `control_domain::Diagnostic`.

## Integration Phases

Phase 1: vendored policy adapter.

- Keep `third_party/mitm_anixops` pinned to an audited tag or commit.
- Build the C core through `crates/mitm-anixops-sys`.
- Expose safe policy helpers through `crates/mitm-policy`.
- Return audit/diagnostics from `MitmPluginService`.
- Cover config diagnostics, MITM decisions, URL rewrite, header rewrite, body
  rewrite, script dispatch, JQ max-input guard, and aggregated rewrite plan in
  upstream GitHub Actions.

Phase 2: domain mutation model.

- Extend NetworkCore domain types so plugin handling can return structured
  request/response mutations.
- Model URL redirect/reject, header add/replace/delete, body replacement, and
  script dispatch output.
- Keep JavaScript execution outside `mitm_anixops`; this library only returns
  script metadata.

Phase 3: native HTTP/TLS path.

- Add an HTTP/TLS MITM boundary to `engine-native` or a selected public engine
  adapter.
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

## Upgrade Procedure

When `mitm_anixops` publishes a new alpha, beta, release candidate, or stable
tag, NetworkCore should update in this order:

1. Read the release notes, `include/mitm_anixops.h`, and `ci/abi_exports.txt`.
2. Move `third_party/mitm_anixops` to the new tag or commit.
3. Update `crates/mitm-anixops-sys` first so unsafe bindings match the C ABI.
4. Update `crates/mitm-policy` so safe Rust types cover new policy outputs.
5. Keep live HTTP/TLS mutation deferred unless the NetworkCore mutation model,
   data plane, and certificate gates have passed.
6. Update NetworkCore adapter docs, release notes, CI summaries, and this
   repository's integration notes.
7. Use GitHub Actions as the acceptance source of truth; local machines remain
   for editing, static inspection, and documentation work only.

## CI Requirements

NetworkCore should prove integration in GitHub Actions before claiming platform
support:

- Linux Rust build/test with the C static library compiled by `cc`;
- macOS Rust build/test with the C static library compiled by `cc`;
- Windows Rust build/test with the C static library compiled by `cc`;
- ABI allowlist comparison against `mitm_anixops/ci/abi_exports.txt`;
- wrapper tests for config diagnostics, MITM decisions, URL rewrites, header
  rewrites, body rewrites, script dispatch, JQ max-input guard, and rewrite
  plans;
- optional iOS `cargo check`, SwiftPM, or Xcode validation on a macOS runner
  after the iOS platform source tree exists.

Until those jobs and the runtime gates pass, call the integration
"policy-core adapter", "adapter-compatible", or "deferred live mutation", not
"complete production MITM".
