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
- [x] Add PR template fields for source contract, test, compatibility matrix,
  changelog, and manual-intervention impact.
- [x] Add a release dry-run design before changing release automation.

## P1 Plugin Parser

- [x] Define Loon plugin source contract for common sections and fields.
- [x] Define Quantumult X source contract for rewrite/task/mitm subsets.
- [ ] Define Surge module source contract for module metadata, scripts, MITM,
  and rewrite subsets.
- [ ] Add one parser fixture and one positive/negative parser test per new
  grammar unit.
- [ ] Extend compatibility matrix rows only after the corresponding CI test
  exists.

## P2 Rule Matching Engine

- [ ] Define trace schema for URL, host, header, body, script trigger, and
  policy intent decisions.
- [ ] Add golden tests proving plan API and legacy evaluate API parity.
- [ ] Add negative tests for malformed hosts, unsupported policy intents, and
  phase mismatches.

## P3 Script And Runtime Compatibility

- [ ] Define script runtime source contract for `$request`, `$response`,
  `$argument`, `$persistentStore`, `$done`, timeout, exception, and double done.
- [ ] Define cron/task trigger as planned until parser and runtime tests exist.
- [ ] Record any QuickJS, JavaScriptCore, or new runtime dependency decision
  before implementation.

## P4 MITM Policy And Certificate Lifecycle

- [ ] Add certificate lifecycle architecture contract.
- [ ] Add manual-intervention markers for CA trust, platform certificate stores,
  protected environments, and signing materials.
- [ ] Add CI checks preventing accidental claims of automatic root trust or
  non-target hostname decryption.

## P5 Integration Adapter

- [ ] Update NetworkCore integration notes for the latest `v0.45.10-alpha`
  baseline and future v1.0.0 adapter boundary.
- [ ] Add binding parity fixtures for C runner, Go wrapper, and Rust wrapper.
- [ ] Define Stash and Shadowrocket compatibility as migration notes unless
  parser fixtures and tests are added.

## P6 Release Hardening

- [ ] Add release dry-run workflow.
- [ ] Add tag-triggered release workflow that builds artifacts in GitHub
  Actions.
- [ ] Generate checksums, manifest, release notes, and release summary in
  GitHub Actions.
- [ ] Add same-commit CI gate before release publication.
- [ ] Add release rollback and replacement policy.
