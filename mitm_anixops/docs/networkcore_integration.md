# NetworkCore Integration Notes

This note records the integration shape for
`https://github.com/AnixOps/networkcore_anixops`.

Inspection date: 2026-07-10.

Inspected upstream `main`:
`e41bde7a07f62d10b0066d500cd9258acd8bfa61`.

Current `mitm_anixops` alpha baseline:
`v0.45.10-alpha`
(`a3ee0fca6376ddccc333bdfe06ac5b5e75ed23e0`).

```text
networkcore-integration-alpha-baseline=v0.45.10-alpha
networkcore-integration-upstream-main=e41bde7a07f62d10b0066d500cd9258acd8bfa61
networkcore-integration-current-mode=vendored-policy-core-plus-plain-http-adapter
networkcore-integration-v1-boundary=adapter-owned-data-plane
networkcore-integration-live-mutation-status=partial-live-http-deferred-https
networkcore-integration-runtime-model=host-plugin-no-standalone-desktop-product
networkcore-integration-final-target=live-https-connect-rewrite-and-script-execution
networkcore-integration-next-host-slice=networkcore-engine-native-tls-session-boundary
networkcore-integration-ci-source-of-truth=github-actions
integration-adapter-readiness-gate=scripts/integration-adapter-readiness-check.sh
integration-adapter-readiness-self-test=scripts/integration-adapter-readiness-check-test.sh
integration-adapter-readiness-status=ci-gated-alpha-boundary
integration-adapter-readiness-release-artifact=policy-core-runner-proxy-shim-bindings-docs
integration-adapter-readiness-release-manifest=adapter-readiness-fields-required
integration-adapter-readiness-production-claim=not-production-networkcore-adapter
```

## Compatibility Conclusion

`mitm_anixops` is a versioned policy/plugin dependency of
`networkcore_anixops`; it is not a separate proxy, desktop client, or
cross-platform network engine. NetworkCore is the only product host for this
integration path.

The inspected NetworkCore repository has moved past the original design-only
state. It now includes:

- `third_party/mitm_anixops`, pinned to the `v0.45.10-alpha` source baseline;
- `crates/mitm-anixops-sys`, the unsafe Rust FFI crate that compiles the C core;
- `crates/mitm-policy`, the safe Rust wrapper and NetworkCore policy adapter;
- `docs/architecture/mitm-anixops-adapter.md`, the upstream adapter design;
- `MitmPluginService` integration that maps policy results into structured
  `HttpMitmOutcome` plans;
- an explicit HTTP proxy path that applies reject, redirect, header, and body
  mutation outcomes to live bounded `http://` HTTP/1.x traffic;
- an explicit CONNECT pass-through and ClientHello/SNI observation foundation,
  plus controlled TLS termination and HTTPS request/response preview reports.

The accurate current statement is:

- NetworkCore can embed the `v0.45.10-alpha` policy core through a vendored C
  ABI and Rust safe wrapper.
- NetworkCore can use the wrapper for config diagnostics, MITM decisions, URL
  rewrite plans, header/body/script policy outputs, and structured mutation
  plans.
- NetworkCore currently applies the non-script subset to its explicit plain
  HTTP path. Live TLS decryption, CONNECT-stream HTTPS mutation, and script
  execution remain blocked.
- NetworkCore still owns the production data plane: socket IO, TLS, certificate
  lifecycle, HTTP parsing/framing, compression, JavaScript runtime, storage,
  platform permissions, and release gates.

## Current Upstream Boundary

The current NetworkCore workspace includes these integration-relevant members:

- `apps/linux-cli`
- `apps/ios`
- `apps/windows-cli`
- `crates/config-core`
- `crates/control-domain`
- `crates/control-runtime`
- `crates/engine-native`
- `crates/engine-singbox`
- `crates/mitm-anixops-sys`
- `crates/mitm-policy`
- `crates/platform-ios`
- `crates/platform-linux`
- `crates/platform-windows`

The active policy insertion point remains:

- `control_domain::MitmPluginService`
- `control_runtime::MitmGateOrchestrator`
- `mitm_policy::AnixOpsMitmPluginService`

That boundary loads plugin/config text, validates supported rule shapes, maps C
diagnostics into NetworkCore diagnostics, and exposes safe wrapper outputs for
contract tests. `engine-native` consumes selected outcomes for live explicit
plain HTTP; HTTPS traffic remains outside the live mutation path.

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

The fixed plugin boundary is:

```text
mitm_anixops = policy-core + C ABI + diagnostics + mutation plans
NetworkCore = adapter + data plane + platform trust + runtime execution
```

## Policy Capability Query Gate

Before NetworkCore or another host consumes an AnixOps MITM decision, rewrite
result, plan, dispatch metadata, diagnostic, or task descriptor, it must verify
the signed managed profile, require the supported policy capability query ABI
version and required flags, reject unknown returned capability bits, enforce
the managed hostname allowlist, and verify host-owned consent and trust state.
If any check fails, the host must reject or bypass the policy result according
to its fail-closed policy.

This gate validates policy-core metadata only. It does not enable or provide
TLS termination, certificate lifecycle, capture, socket IO, HTTP framing,
HTTP/2, QUIC, JavaScript execution, storage, permissions, or consent.

## Current NetworkCore Gaps

NetworkCore now has a partial command surface, certificate artifact lifecycle,
browser-capture planning, live explicit plain HTTP mutation, and CONNECT
pass-through. It still must not be described as user-facing full MITM. The
current upstream gates are:

- `MITM_CLI_COMMAND_GATE=partial-active`: status, diagnostics, certificate,
  browser-capture, and HTTP rewrite command surfaces exist, but no standalone
  `mitm_anixops` client is expected or required.
- `MITM_CERTIFICATE_LIFECYCLE_GATE=artifact-lifecycle-active/profile-trust-artifact-active/trust-mutation-blocked`:
  NetworkCore can create rollback-aware PEM artifacts, but CA installation,
  trust detection, revocation, and trust-store rollback are still blocked.
- `MITM_BROWSER_CAPTURE_GATE=pac-policy-profile-prefs-active/system-mutation-blocked`:
  dedicated-profile/PAC artifacts and verification plans exist, but system
  proxy/PAC mutation and live-capture proof remain blocked.
- `MITM_HTTP_TLS_DATA_PLANE_GATE=plain-http-live-data-plane-active/tls-decryption-blocked`:
  bounded explicit plain HTTP mutation is live; live TLS termination,
  decrypted HTTPS request/response mutation, framing/compression, and script
  execution are still blocked.

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

## Plugin Delivery Plan

This repository is planned as a NetworkCore plugin source, not as an
independent desktop runtime. A proxy shim remains a reference/contract fixture
only; it is not a competing NetworkCore data plane.

The final NetworkCore plugin target explicitly includes live HTTPS decryption,
CONNECT-stream request/response rewrite, and script execution. Their ownership
by NetworkCore is a placement decision, not a deferral or non-goal.

### Phase 0: Releaseable policy-core snapshots

- Finish one ABI-compatible policy increment at a time, with C, Go, Rust, and
  compatibility evidence before a tag.
- Publish the header, `ci/abi_exports.txt`, changelog, fixtures, and semantic
  limits together. Do not ask NetworkCore to vendor a working-tree commit.
- Treat explicit-length body APIs, binary fail-open behavior, JQ limits, and
  rule diagnostics as plugin inputs that must retain stable adapter semantics.

Exit evidence: a tagged `mitm_anixops` release with green CI and a documented
ABI delta; no change is claimed live in NetworkCore before its vendor update.

### Phase 1: NetworkCore vendor and wrapper parity

- Move `third_party/mitm_anixops`, then update `mitm-anixops-sys` and
  `mitm-policy` in that order.
- Map every newly consumed C result into NetworkCore-owned Rust types and
  preserve bytes lengths rather than converting binary bodies through strings.
- Add shared-fixture parity tests for config diagnostics, host decisions, URL,
  header, ordered body-chain, script dispatch, and `RewritePlan` outputs.
- Keep the rich `HttpMitmOutcome` model authoritative; do not recreate a
  second mutation model in this repository.

Exit evidence: upstream CI compiles the pinned ABI on every supported host and
proves the same policy outcome from the shared fixture in C and Rust.

### Phase 2: Host-owned live HTTPS data plane

- Implement this only in NetworkCore `engine-native` (or its selected host
  engine): controlled downstream TLS termination, per-host leaf issuance,
  upstream TLS forwarding, and strict HTTP parser/framing guards.
- Apply the already-existing policy outcomes in request and response order;
  treat `ANIXOPS_MITM_REJECT_QUIC` as a host-level TCP fallback signal.
- Use bounded byte APIs for body mutation and make decompression,
  recompression/identity writeback, charset, `Content-Length`, and
  `Transfer-Encoding` NetworkCore responsibilities.

Exit evidence: the upstream HTTP/TLS gate moves from
`tls-decryption-blocked` only after live CONNECT-stream HTTPS request and
response tests prove no bypass, no framing corruption, and fail-open handling.

#### Phase 2 implementation sequence

1. **TLS session boundary.** Add a NetworkCore-owned `engine-native` TLS MITM
   session interface that receives an already-authorized CONNECT socket,
   CONNECT authority, and CA material. It must reject CONNECT/SNI authority
   disagreement, issue a short-lived per-host leaf certificate, terminate the
   downstream handshake, and establish a separately verified upstream TLS
   connection. This first slice must be opt-in, loopback-only, HTTP/1.1-only,
   and observable; it must not alter platform trust stores.
2. **Strict CONNECT HTTP/1.1 loop.** On the two TLS streams, add bounded
   request/response parsing with explicit header/body limits, single framing
   interpretation, and rejection of ambiguous `Content-Length` /
   `Transfer-Encoding` messages. Feed only fully parsed, bounded messages into
   the policy adapter; relay opaque or unsupported traffic without pretending
   it was rewritten.
3. **Live mutation application.** Invoke `HttpMitmOutcome` in request order
   (terminal URL action, headers, body, script dispatch) and response order
   (headers, body, script dispatch). Use the explicit-length bytes body APIs;
   retain binary bodies unchanged; make decoded-body writeback recompute
   `Content-Length` and remove or regenerate `Content-Encoding` and
   `Transfer-Encoding` consistently.
4. **Protocol and failure gates.** Begin with identity/gzip/deflate bodies and
   HTTP/1.1. Keep HTTP/2, brotli, zstd, WebSocket upgrades, streaming bodies,
   and unsupported charsets on an explicit bypass/diagnostic path until each
   has its own contract. Any TLS, parser, body-limit, policy, or upstream
   failure must choose the documented fail-open tunnel or fail-closed CONNECT
   response; it must never emit a partially rewritten message.

The first code change for this phase is the TLS session boundary plus an
in-memory loopback handshake test. It has no script engine and no live body
rewrite yet; that deliberate boundary lets the certificate, SNI, downstream,
and upstream TLS invariants be verified before request mutation is enabled.

### Phase 3: Plugin runtime completion in the host

- Select and sandbox the NetworkCore JavaScript runtime; use the policy core
  only for dispatch metadata.
- Add NetworkCore-owned script fetch/cache refresh, cancellation, quotas,
  persistent-store locking, and redacted trace behavior.
- Complete the optional JQ runtime policy with host-side cache refresh and
  recursion/iteration defense; retain the C-core byte/time/memory limits.

Exit evidence: scripts and JQ run against real host traffic with timeout,
exception, binary-body, compression, and persistence tests; raw payload trace
export remains opt-in and redacted by default.

#### Phase 3 implementation sequence

1. Define a NetworkCore `ScriptRuntime` interface whose input and output are
   the existing `HttpMitmScriptDispatch` plus bounded request/response views;
   it owns `$request`, `$response`, `$argument`, `$done`, timeout, cancellation,
   and error classification.
2. Select the embedded JavaScript engine in NetworkCore through a dependency
   decision record, then implement it behind that interface. Do not embed an
   engine or fetch remote scripts from `mitm_anixops`.
3. Add an adapter-owned persistent-store interface with namespace, locking,
   quota, atomic-write, and migration rules. Script execution defaults to
   fail-open after static mutations, matching the existing policy contract.
4. Connect the runtime only after the live HTTPS message loop is stable, first
   for bounded UTF-8/JSON bodies and then for explicitly contracted binary and
   compressed body behavior. Add a real CONNECT-stream request-script and
   response-script E2E before enabling each phase.

### Phase 4: Host capability gates and plugin acceptance

- Advance certificate trust, browser/application capture, and platform
  permissions only in the relevant NetworkCore host adapter.
- Define a supported plugin corpus by version and reject/diagnose unsupported
  grammar deterministically; do not claim all Loon/Surge/QX plugins.
- Promote a host only after its CI proves the same plugin corpus, TLS path,
  rollback behavior, and security defaults. This repository itself has no
  desktop-GUI, installer, or standalone-proxy deliverable.

Exit evidence: the relevant NetworkCore gate is active with platform-specific
tests and the plugin version is pinned in its release metadata.

## Upgrade Procedure

When `mitm_anixops` publishes a new alpha, beta, release candidate, or stable
tag, NetworkCore should update in this order:

1. Read the release notes, `include/mitm_anixops.h`, and `ci/abi_exports.txt`.
2. Move `third_party/mitm_anixops` to the new tag or commit.
3. Update `crates/mitm-anixops-sys` first so unsafe bindings match the C ABI.
4. Update `crates/mitm-policy` so safe Rust types cover new policy outputs.
5. Retain the current partial plain HTTP path, but keep HTTPS mutation and
   script execution blocked until the relevant NetworkCore data-plane and
   certificate gates pass.
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

Until those jobs and the runtime gates pass, call the integration a
"NetworkCore policy plugin" or "partial plain-HTTP adapter", not "complete
production MITM".
