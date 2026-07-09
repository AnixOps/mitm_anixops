# mitm_anixops v1.0.0 Roadmap

This roadmap moves `mitm_anixops` from the current Alpha policy core into a
mature, testable, releasable MITM plugin compatibility layer.

The project follows the same release-engineering posture as
`networkcore_anixops`: local machines are for editing source and documentation;
build, test, package, release dry-run, and release publication must be proven by
GitHub Actions.

## Current Phase

Current track: v1.3.x post-publication evidence hardening on the path to v3.0.0
production MITM readiness.

roadmap-current-track=v1.0.0-evidence-hardening
roadmap-p0-baseline-status=complete
roadmap-v1-release-status=ready-for-tag-after-ci

The P0 repository baseline is complete. Foundation work now exists across P1
through P6, and the repository-level `v1.0.0` publication blockers have
confirmed external evidence in `docs/manual-intervention.md`. The project is
ready for a `v1.0.0` tag after the release commit has green GitHub Actions
evidence and the tag-triggered release workflow passes its environment gate.
Compatibility matrix rows still include documented `partial` scope and
`unsupported` guard rows; those are release notes scope limits, not hidden
supported claims.

The latest published stable artifact is `v1.3.3`. The `v1.1.x` train is
reserved for script and cron/task trigger compatibility, covering deterministic
body trigger dispatch metadata, direct trigger attribute-boundary parsing, CI
evidence gates, parser boundary hardening for task descriptor metadata, and
structured script runtime error reporting plus script runtime security review
gates. The `v1.2.0` milestone closes the integration adapter usability loop by
making NetworkCore boundary notes, alpha package contents, runner/proxy shim
targets, binding/package checks, E2E entrypoints, and release registrations
part of a CI-verifiable readiness gate without claiming production
NetworkCore data-plane support. The `v1.2.x` train then hardens release
metadata so generated manifests, release notes, and summaries carry a stable
manifest schema identity plus the same adapter readiness status, gate, scope,
production-boundary evidence, SHA-256 digest/sidecar format semantics, release
workflow run traceability, explicit publication-gate evidence, and source-mode
evidence that distinguishes tag publication, manual validation, main-push
dry-run, and pull-request dry-run paths, then aligns CI run ID, URL,
conclusion, artifact count, and artifact platform evidence across release and
dry-run manifests, notes, and summaries. The `v1.3.0` milestone starts the
post-publication verification track by making public GitHub Release asset
checks reproducible through a script that validates assets, manifest fields,
checksum sidecars, release notes, CI run evidence, release workflow run
evidence, artifact count, and artifact platforms. The `v1.3.1` follow-up adds
offline fixture coverage for the publication verifier's success and checksum
mismatch paths before relying on it for newer public releases. The `v1.3.2`
follow-up archives the post-publication verifier output as a workflow artifact
so released tags keep downloadable verification evidence. The `v1.3.3`
follow-up records that evidence in a machine-readable release evidence index
that points to the public release, commit, CI run, release run, artifact
counts, platforms, and publication evidence artifact. The `v1.3.4` follow-up
refreshes that index to the latest published stable tag, preserves the previous
stable release evidence entry, and requires release notes to carry explicit
feature additions and BUG fixes sections.

## Production MITM Version Line

production-grade MITM正式声明只允许从 `v3.0.0` 开始。Earlier releases
may build evidence, adapter contracts, beta packages, and RC candidates, but
they must keep release notes and manifests clear about alpha, beta, or RC
scope.

```text
roadmap-production-mitm-gate=scripts/production-mitm-roadmap-check.sh
roadmap-production-mitm-contract-freeze-version=v2.0.0
roadmap-production-mitm-beta-version=v2.8.0
roadmap-production-mitm-rc-version=v2.9.0
roadmap-production-mitm-ga-version=v3.0.0
roadmap-production-mitm-pre-v3-claim=not-production-ready
```

- `v2.0.0` freezes the production adapter contract: C ABI, runner/proxy shim
  surfaces, adapter-owned network/TLS responsibilities, manifest fields, and
  release evidence vocabulary are stable enough for real adapter work, but it
  carries no production-ready MITM claim.
- `v2.1.x` through `v2.7.x` build the production path: real adapter skeleton,
  certificate lifecycle handoff, MITM rule policy, observable interception
  traces, configuration rollback, stability gates, and security defaults.
- `v2.8.0 beta` is the first production-grade MITM beta. It may be used for
  controlled real-world validation, but release metadata must still say beta.
- `v2.9.0 RC` is the release-candidate line. New feature scope is frozen; only
  blocker fixes and evidence corrections are allowed.
- `v3.0.0 production-ready` is the first GA production MITM release and the
  first version allowed to remove alpha/beta/RC production-readiness limits.

## Operating Rules

- Work in small increments.
- One feature unit gets one contract or test before implementation.
- Do not remove meaningful tests to make CI pass.
- Do not claim planned, placeholder, or unverified capabilities as supported.
- Every user-visible behavior change updates `CHANGELOG.md`.
- Every compatibility change updates `docs/compatibility/`.
- Every security-sensitive or platform-dependent item that cannot be automated
  is recorded in `docs/manual-intervention.md`.
- GitHub Actions is the acceptance source of truth.

## P0 Repository Baseline

Goal: make the repository maintainable before adding more compatibility surface.

Deliverables:

- Repository audit and current-state inventory.
- v1.0.0 roadmap and TODO baseline.
- CONTRIBUTING and CHANGELOG.
- Architecture documentation skeleton.
- Compatibility documentation skeleton.
- Manual intervention register.
- CI governance check proving the baseline files exist.

Exit criteria:

- GitHub Actions verifies the governance baseline.
- `README.md`, `ROADMAP.md`, `TODO.md`, `CHANGELOG.md`, `CONTRIBUTING.md`,
  `docs/architecture/`, `docs/compatibility/`, and
  `docs/manual-intervention.md` are present and internally linked.
- The audit states current tags, releases, CI, tests, entry points, and gaps.

## P1 Plugin Parser

Goal: stabilize parser behavior for the high-frequency plugin subset.

Scope:

- Loon plugin sections and common fields.
- Quantumult X rewrite/task/mitm subset.
- Surge module subset.
- Stash `http.mitm` and `http.url-rewrite` request URL subsets, Shadowrocket
  common-config subset, plus Stash and Shadowrocket app-profile migration notes
  where syntax overlaps.
- Malformed-line behavior by compatibility profile.

Required evidence:

- Source contract for each accepted grammar family.
- Parser fixture for every new grammar unit.
- Positive and negative tests in CI.
- Compatibility matrix row updated for every changed capability.
- Diagnostics for ignored, partial, rejected, and unsupported rules.

## P2 Rule Matching Engine

Goal: make URL, host, header, body, script-trigger, and policy-intent matching
deterministic and traceable.

Scope:

- hostname and MITM domain matching.
- request rewrite.
- response rewrite.
- header mutation.
- body mutation.
- reject/direct/proxy policy intent as structured intent, not packet routing.
- stable trace schema for runner and adapter parity.

Required evidence:

- Contract-first matching semantics.
- Golden positive and negative fixtures.
- Plan API parity checks against legacy evaluate APIs.
- Compatibility matrix updated with supported, partial, planned when present,
  and unsupported.

## P3 Script And Runtime Compatibility

Goal: define and verify script dispatch/runtime behavior without hiding adapter
responsibilities inside the C policy core.

Scope:

- script trigger selection and metadata.
- `$request`, `$response`, `$argument`, `$persistentStore`, `$done` contract.
- async, timeout, exception, double-done, max-size, and fail-open behavior.
- offline script bundle, digest, cache miss, and future remote-cache policy.
- cron/task descriptor parsing before scheduler/runtime implementation.

Required evidence:

- Node runner contract remains as Alpha/demo backend until a production runtime
  is introduced.
- Any QuickJS, JavaScriptCore, or other runtime backend gets independent
  contracts, tests, license review, and release notes.
- No remote script execution is enabled by default.

## P4 MITM Policy And Certificate Lifecycle

Goal: keep MITM security boundaries conservative and explicit.

Scope:

- MITM host policy and QUIC fallback intent.
- certificate state model.
- certificate lifecycle source contracts for adapters.
- no automatic root trust.
- no decryption outside target hostnames.
- no sensitive header/body logging by default.

Required evidence:

- Certificate states are explicit inputs to the C ABI or adapter contract.
- Platform certificate installation and trust UX remain adapter-owned.
- Any CA, leaf certificate, key storage, trust, revocation, or platform
  permission behavior has manual-intervention entries where automation cannot
  prove it.

## P5 Integration Adapter

Goal: make `mitm_anixops` embeddable by NetworkCore and other host adapters
without turning it into a GUI or platform network stack.

Scope:

- C ABI stability and ABI allowlist.
- Go, Rust, CMake, and pkg-config integration.
- no-UI runner integration.
- NetworkCore adapter contract alignment.
- platform adapter samples only after source contracts and CI gates exist.

Required evidence:

- Wrapper tests in GitHub Actions.
- Same fixture produces comparable traces through C runner and bindings.
- Adapter docs state what remains owned by the host: network IO, TLS, HTTP/2,
  compression/framing, script runtime, certificate store, and platform policy.

## P6 Release Hardening

Goal: make v1.0.0 release publication reproducible and blocked on evidence.

Scope:

- GitHub Actions lint, format check, unit tests, parser fixtures,
  compatibility matrix tests, integration smoke tests, and release dry-run.
- Tag-triggered release workflow.
- Artifacts, checksums, manifest, release notes, and upload gates.
- Same-commit CI gate before publication.
- Rollback and replacement policy.

Required evidence:

- Release workflow builds artifacts in GitHub Actions, never from local output.
- Release assets include checksums and a manifest.
- Release notes state supported, partial, planned count, unsupported, and
  known-risk areas without overclaiming.
- v1.0.0 is published only after all v1.0.0 minimum acceptance criteria pass.

## v1.0.0 Minimum Acceptance Criteria

- README explains purpose, installation, compatibility scope, and limitations.
- ROADMAP, TODO, CHANGELOG, and compatibility docs match the actual state.
- Compatibility matrix clearly marks supported, partial, planned when present,
  and unsupported.
- Parser, rule engine, MITM policy, and script trigger have stable CI coverage.
- GitHub Actions passes on PR, main push, and tag push.
- Release workflow creates artifacts, checksum files, manifest, and release
  notes, and blocks publication on failure.
- Manual intervention items are recorded and do not silently block or bypass
  automation.
- No planned or placeholder capability is described as complete.
