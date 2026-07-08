# TODO

This file tracks the next small increments toward `v1.0.0`.

Long-term direction is in [ROADMAP.md](ROADMAP.md). Compatibility scope is in
[docs/compatibility/](docs/compatibility/). Alpha-stage library gaps remain in
[docs/TODO.md](docs/TODO.md).

## P0 Repository Baseline

- [x] Publish the latest Alpha release before starting the v1.0.0 track.
- [x] Add a repository audit and architecture baseline.
- [x] Add v1.0.0 roadmap, TODO, CHANGELOG, CONTRIBUTING, compatibility docs,
  and manual intervention register.
- [x] Add CI governance checks for the new baseline files.
- [x] Add a dedicated compatibility matrix CI job.
- [x] Add a dedicated static format check CI job.
- [x] Add a dedicated static lint CI job.
- [x] Add a macOS policy-core smoke CI job.
- [x] Add PR template fields for source contract, test, compatibility matrix,
  changelog, and manual-intervention impact.
- [x] Add a release dry-run design before changing release automation.

## P1 Plugin Parser

- [x] Define Loon plugin source contract for common sections and fields.
- [x] Define Quantumult X source contract for rewrite/task/mitm subsets.
- [x] Define Surge module source contract for module metadata, scripts, MITM,
  and rewrite subsets.
- [x] Add Loon hashbang metadata parser fixture and positive/negative tests.
- [x] Add Shadowrocket common-config parser fixture and positive/negative tests.
- [x] Add reject/direct/proxy policy-intent parser fixtures and
  positive/negative tests.
- [x] Add cron/task non-support parser fixtures and guard tests.
- [ ] Add one parser fixture and one positive/negative parser test per new
  grammar unit.
- [ ] Extend compatibility matrix rows only after the corresponding CI test
  exists.

## P2 Rule Matching Engine

- [x] Define request rewrite source contract with parser fixtures and CI-covered
  positive/negative tests.
- [x] Define header mutation source contract with parser fixtures and CI-covered
  positive/negative tests.
- [x] Define response rewrite source contract with parser fixtures and
  CI-covered positive/negative tests.
- [x] Define body mutation source contract with parser fixtures and CI-covered
  positive/negative tests.
- [x] Define trace schema for URL, host, header, body, script trigger, and
  policy intent decisions.
- [x] Add runner MITM decision golden JSON trace fixtures for TCP allow and
  QUIC fallback.
- [x] Add runner negative MITM decision golden JSON trace fixtures for
  certificate, deny-host, and no-host-match bypasses.
- [x] Add runner disabled and malformed MITM decision golden JSON trace
  fixtures.
- [x] Add negative fixture proving unsupported direct/proxy policy intent stays
  ignored until routing semantics are contracted.
- [x] Define reject/direct/proxy policy-intent contract with CI-covered reject
  subset and unsupported route tests.
- [x] Add golden tests proving plan API and legacy evaluate API parity.
- [x] Add current-header plan parity checks for C, Go, and Rust surfaces.
- [x] Add negative tests for phase mismatches.
- [x] Add negative tests for malformed hosts.

## P3 Script And Runtime Compatibility

- [x] Define script runtime source contract for `$request`, `$response`,
  `$argument`, `$persistentStore`, `$done`, timeout, exception, and double done.
- [x] Define cron/task trigger as parser metadata until runtime tests exist.
- [x] Add CI-covered parser guards proving unsupported cron/task-like lines do
  not register as HTTP script triggers.
- [x] Add cron/task descriptor parser fixtures, public ABI, and
  positive/negative tests.
- [x] Record any QuickJS, JavaScriptCore, or new runtime dependency decision
  before implementation.

## P4 MITM Policy And Certificate Lifecycle

- [x] Add certificate lifecycle architecture contract.
- [x] Add manual-intervention markers for CA trust, platform certificate stores,
  protected environments, and signing materials.
- [x] Add CI checks preventing accidental claims of automatic root trust or
  non-target hostname decryption.

## P5 Integration Adapter

- [x] Update NetworkCore integration notes for the latest `v0.45.10-alpha`
  baseline and future v1.0.0 adapter boundary.
- [x] Add binding parity fixtures for C runner, Go wrapper, and Rust wrapper.
- [x] Add named-header current-value parity checks for Go and Rust wrappers.
- [x] Add runner golden JSON trace fixtures for binding parity.
- [x] Define Stash and Shadowrocket app-profile compatibility as migration
  notes unless parser fixtures and tests are added.
- [x] Add Stash and Shadowrocket migration guard fixtures and parser tests.

## P6 Release Hardening

- [x] Add release dry-run workflow.
- [x] Add tag-triggered release workflow that builds artifacts in GitHub
  Actions.
- [x] Generate checksums, manifest, release notes, and release summary in
  GitHub Actions.
- [x] Add same-commit CI gate before release publication.
- [x] Add release rollback and replacement policy.
- [x] Add tag-only GitHub Release asset publication after release gates pass.
