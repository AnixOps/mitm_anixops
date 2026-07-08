# Repository Audit

Audit date: 2026-07-08.

This audit establishes the P0 baseline for the `v1.0.0` roadmap. It is a current
state inventory, not a compatibility claim.

## Repository Shape

The Git repository root is `/root/crack/loon`; `mitm_anixops` is the maintained
subproject under `mitm_anixops/`.

Important paths:

- `.github/workflows/build.yml`: active GitHub Actions workflow.
- `.github/workflows/release-dry-run.yml`: release dry-run workflow that blocks
  publication and uploads workflow artifacts only.
- `.github/workflows/release.yml`: tag-triggered release workflow that builds
  artifacts in GitHub Actions and publishes GitHub Release assets after gates.
- `mitm_anixops/include/`: public C ABI.
- `mitm_anixops/src/`: C implementation.
- `mitm_anixops/tests/`: C unit and compatibility fixture tests.
- `mitm_anixops/tools/`: no-UI runner.
- `mitm_anixops/e2e/`: Go proxy shim and script-runtime E2E scripts.
- `mitm_anixops/bindings/go/`: Go cgo wrapper.
- `mitm_anixops/bindings/rust/`: Rust FFI wrapper.
- `mitm_anixops/cmake/` and `mitm_anixops/pkgconfig/`: C consumer package
  metadata.
- `mitm_anixops/docs/`: compatibility, release, runtime, and roadmap docs.
- `mitm_anixops/docs/architecture/release-rollback-policy.md`: immutable tag,
  asset, rollback, and replacement rules for release automation.

## Language And Toolchain Stack

Current stack:

- C99 policy core and runner.
- POSIX shell scripts for CI entrypoints and E2E orchestration.
- Go for the Alpha proxy shim.
- JavaScript for the Node-based script contract runner and fixtures.
- Rust wrapper crate using pkg-config.
- CMake and pkg-config consumer smoke coverage.
- Optional PCRE2 backend when built with `PCRE2=1`.
- Optional libjq backend when built with `JQ=1`.

Default library builds are intended to keep third-party dependencies out of the
policy core ABI.

## Entry Points

Public and operational entry points:

- `include/mitm_anixops.h`: public ABI.
- `src/mitm_anixops.c`: implementation.
- `Makefile`: local and CI build targets.
- `scripts/check.sh`: Linux full-check entrypoint used by GitHub Actions.
- `tools/anixops_mitm_runner.c`: `scan`, `scan --corpus`, `trace`, and
  `replay`.
- `e2e/shim/main.go`: Alpha HTTP/1.1 CONNECT/TLS proxy shim.
- `e2e/script_runtime/anixops_runner.js`: Node contract runner.

## CI/CD Status

Active workflows:

- `.github/workflows/build.yml`
- `.github/workflows/release-dry-run.yml`
- `.github/workflows/release.yml`

Current jobs:

- `linux-test`: installs CMake, pkg-config, PCRE2, libjq, Go, Node, and the
  pinned mihomo fixture; runs `sh scripts/check.sh`; builds Alpha tarball;
  uploads `anixops-mitm-alpha-linux`.
- `compatibility-matrix`: validates the v1.0.0 compatibility matrix row
  structure, allowed status values, source contract links, and required evidence
  fields without running a build.
- `format-check`: validates tracked text files for trailing whitespace, CRLF,
  final newlines, and POSIX shell syntax without running a build.
- `lint`: runs `shellcheck` against repository shell scripts at error severity.
- `macos-smoke`: runs default C tests, demo checks, runner checks, and
  proxy-shim build checks on a macOS runner without producing release artifacts.
- `windows-binary`: builds the Windows Bilibili demo binary with MSYS2 and
  uploads `anixops-mitm-windows-x64`.
- `release-dry-run`: validates a dry-run version, runs an equivalent full check
  in GitHub Actions, builds a dry-run artifact, and uploads checksum, manifest,
  release notes, and summary evidence as workflow artifacts without public
  release publication.
- `release`: validates `v*` tags or manual release workflow checks from `main`,
  requires a successful same-commit `build.yml` run on `main`, runs the release
  build gate in GitHub Actions, validates rollback/replacement policy, builds
  the release package, and uploads it with checksum, manifest, release-note, and
  summary evidence as workflow artifacts. For `v*` tag runs, it can publish
  those workflow-generated assets to a GitHub Release after the release gates
  and `github-release-publication` environment gate pass.

Known CI/CD gaps for v1.0.0:

- GitHub Release upload exists only for gated `v*` tag runs; release
  environment approval still needs repository configuration.

The old `mitm_anixops/ci/github-actions.yml` file is a legacy workflow snippet,
not the active GitHub workflow path.

## Tags And Releases

Observed tags:

- `v0.45.10-alpha`
- `v0.41.0-alpha`

Observed GitHub releases:

- `mitm_anixops Alpha 0.45.10`, pre-release, tag `v0.45.10-alpha`.
- `mitm_anixops Alpha 0.41.0`, pre-release, tag `v0.41.0-alpha`.

The `v0.45.10-alpha` release contains:

- `anixops-mitm-alpha-0.45.10.tar.gz`
- `anixops-mitm-windows-x64.zip`

## Test Status

Current test assets include:

- C harness registered through `tests/test_main.c`.
- ABI tests.
- config/parser diagnostics tests.
- MITM host, certificate state, wildcard, deny, and QUIC decision tests.
- URL/header/body rewrite tests.
- script dispatch tests.
- fixture corpus manifest with Loon, Surge, Quantumult X, and BiliUniverse
  fixtures.
- runner scan/trace/replay checks.
- proxy shim, script contract, BiliUniverse, and Bilibili homepage E2E scripts.
- ABI export allowlist check.
- pkg-config, CMake, Go wrapper, and Rust wrapper checks.

Source inspection currently shows 116 registered C test entries. Actual pass/fail
status must be taken from GitHub Actions, not local execution.

## Current Strengths

- Compact C ABI with explicit result structs.
- Strong Alpha fixture coverage for policy-core behavior.
- Runner and proxy shim prove no-UI diagnostics and selected integration paths.
- Optional PCRE2 and libjq backends are gated from the default build.
- Go/Rust/CMake/pkg-config integration has existing coverage.
- Release notes and compatibility docs already avoid claiming full Loon parity.

## Current Gaps

- v1.0.0 governance files were incomplete before this P0 baseline.
- New parser grammar units still need one fixture and positive/negative tests
  before matrix rows can be promoted.
- Release environment approval is documented, but repository environment
  protection still requires manual configuration.
- No macOS release artifact, signing, notarization, entitlement, or platform
  adapter coverage yet.
- Stash and Shadowrocket are not yet first-class parser targets.
- Cron/task trigger behavior has a planned source contract, but parser fixtures,
  scheduler dispatch, and runtime execution remain unimplemented.
- Production JS runtime remains out of scope for the C policy core; the
  dependency decision is recorded in `docs/architecture/script-runtime-dependency.md`.
- Certificate lifecycle is adapter-owned; the v1.0.0 policy-core contract is
  documented in `docs/architecture/certificate-lifecycle.md`, while production
  CA stores, signing materials, and trust UX remain pending manual-intervention
  items.

## Upstream Reference

`networkcore_anixops` provides useful governance patterns:

- CI/CD Only rule: local edits, GitHub Actions verification.
- source-contract-first development;
- `docs/manual-intervention.md` as the record for external blockers;
- release workflow with policy, same-commit CI gate, artifact contract,
  checksum/manifest, attestation, release notes, and publish eligibility gates;
- explicit separation between MITM policy core and platform data plane.

The upstream MITM adapter design treats `mitm_anixops` as a policy/plugin
compatibility backend, not as a full TLS, HTTP, certificate, or JS runtime.
