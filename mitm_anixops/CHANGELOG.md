# Changelog

All notable changes to `mitm_anixops` are recorded here.

The format follows a simple Keep-a-Changelog style. Releases use tags such as
`v0.45.10-alpha` and future stable releases use `vMAJOR.MINOR.PATCH`.

## Unreleased

### Added

- Started the v1.0.0 governance track with repository audit, roadmap,
  compatibility documentation skeleton, contribution rules, and manual
  intervention register.
- Added a pull request template that requires source contract, test,
  compatibility matrix, changelog, security, and manual-intervention evidence.
- Added a dedicated compatibility matrix GitHub Actions job that validates
  matrix row structure, status values, source contracts, and required evidence.
- Added a dedicated static format check GitHub Actions job for whitespace,
  CRLF, final newline, and shell syntax checks.
- Added a release dry-run source contract before introducing release automation.
- Added a Loon common-fields source contract plus positive and negative parser
  fixtures for the first P1 parser milestone.
- Added a Loon hashbang metadata source contract plus positive and negative
  parser fixtures for tolerated `#!` metadata diagnostics.
- Added a Quantumult X rewrite/MITM common-config source contract plus positive
  and negative parser fixtures for the second P1 parser milestone.
- Added a Surge module common-config source contract plus positive and negative
  parser fixtures for the third P1 parser milestone.
- Added a request rewrite source contract plus positive and negative parser
  fixtures for the first P2 rule-matching milestone.
- Added a header mutation source contract plus positive and negative parser
  fixtures for the second P2 rule-matching milestone.
- Added a response rewrite source contract plus positive and negative parser
  fixtures for the third P2 rule-matching milestone.
- Added a body mutation source contract plus positive and negative parser
  fixtures for the fourth P2 rule-matching milestone.
- Added a decision trace schema source contract plus CI-covered fixtures for
  structured MITM, rewrite, mutation, script, and policy-intent evidence.
- Added a reject/direct/proxy policy-intent source contract plus CI-covered
  fixtures proving the reject subset and unsupported direct/proxy route
  boundary.
- Added plan API parity fixtures and tests proving plan aggregation matches
  legacy URL/body/header/script evaluation for matched and mismatched phases.
- Added malformed MITM hostname parser and runtime negative tests so invalid
  hosts do not register or intercept through wildcard policy.
- Added a script runtime common source contract plus no-network replay evidence
  for double `$done` first-wins behavior.
- Added a planned cron/task trigger source contract that keeps scheduler
  support out of the supported surface until parser/runtime evidence exists.
- Added cron/task parser guard fixtures proving scheduler-like lines do not
  register as HTTP script triggers while cron/task support remains planned.
- Added a script runtime dependency decision recording no embedded QuickJS,
  JavaScriptCore, or other production JavaScript engine in the v1 policy core.
- Added a certificate lifecycle architecture contract, manual-intervention
  markers, and CI security-claim checks for root trust and target-host safety.
- Added updated NetworkCore integration notes for the `v0.45.10-alpha`
  adapter baseline, deferred live-mutation gates, and v1.0.0 host-owned data
  plane boundary.
- Added a shared binding parity fixture with C runner, Go wrapper, and Rust
  wrapper CI coverage.
- Added Stash and Shadowrocket migration notes that keep both ecosystems
  planned until dedicated parser fixtures and tests exist.
- Added Stash and Shadowrocket migration guard fixtures proving app-level
  profile syntax remains ignored until dedicated parser support exists.
- Added a dedicated GitHub Actions release dry-run workflow that runs a
  same-workflow CI gate, builds dry-run artifacts, and generates checksum,
  manifest, release-note, and summary evidence without publishing a release.
- Added a tag-triggered GitHub Actions release workflow that builds release
  package artifacts.
- Added checksum, manifest, release-note, and Step Summary generation to the
  release workflow artifacts.
- Added a same-commit `build.yml` success gate before the release workflow can
  package release artifacts.
- Added a release rollback and replacement policy with immutable public tag and
  asset rules, enforced by the release workflow policy stage.
- Added tag-only GitHub Release publication for workflow-generated release
  artifacts after same-commit CI, metadata, immutable-release, and environment
  gates pass.

### Changed

- Clarified that GitHub Actions is the acceptance source of truth for build,
  test, package, release dry-run, and release publication.

## v0.45.10-alpha - 2026-07-08

### Added

- Published Alpha artifacts for Linux and Windows.
- Added runner and proxy-shim coverage for script exception fail-open behavior.
- Included script-runtime error fixture in the Alpha package.

### Notes

- This Alpha proves a policy-core and no-UI runner foundation. It is not a
  v1.0.0 compatibility claim.
