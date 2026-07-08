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
- Added a release dry-run source contract before introducing release automation.
- Added a Loon common-fields source contract plus positive and negative parser
  fixtures for the first P1 parser milestone.
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
- Added plan API parity fixtures and tests proving plan aggregation matches
  legacy URL/body/header/script evaluation for matched and mismatched phases.
- Added malformed MITM hostname parser and runtime negative tests so invalid
  hosts do not register or intercept through wildcard policy.

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
