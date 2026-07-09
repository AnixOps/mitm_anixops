# Changelog

All notable changes to `mitm_anixops` are recorded here.

The format follows a simple Keep-a-Changelog style. Releases use tags such as
`v0.45.10-alpha` and future stable releases use `vMAJOR.MINOR.PATCH`.

## Unreleased

### Added

- Added a `v1.1.5` script runtime security review gate that requires the
  no-embedded-engine decision, pending production runtime/redaction markers,
  script runtime security boundary markers, and matrix/source-contract evidence
  before CI or release readiness can pass.
- Added a `v1.1.4` script runtime error reporting guard so Alpha replay
  `scriptRuntime` output exposes stable `errorKind` and `errorMessage` fields
  for exception and timeout fail-open paths.
- Added a `v1.1.3` cron/task compatibility guard so quoted attr-list task
  `type` values such as `type="cron"`, `type='interval'`, and `type="task"`
  register as task descriptors while remaining outside HTTP script URL
  dispatch.
- Added a `v1.1.2` CI/CD script trigger evidence gate that scans registered
  `tests/test_script.c` `script/...` tests and requires each one to be recorded
  in both the script trigger compatibility matrix row and source-contract
  evidence before CI can pass.
- Added a `v1.1.1` script trigger attribute-boundary compatibility guard so
  direct url-prefixed script paths may be followed immediately by comma-delimited
  `requires_body`/`requires-body`, `timeout_ms`/`timeout-ms`, and
  `max_size`/`max-size` attributes with quoted or spaced values without leaking
  the comma into `script_path`.
- Added a `v1.1.0` script trigger compatibility guard so Quantumult X
  `script-request-body` and `script-response-body` tokens always expose
  `requires_body=1` in dispatch metadata, even when a later attr-list segment
  tries to override the body requirement.
- Added a `v1.0.5` MITM hostname policy hardening test and matcher update:
  hostnames now normalize trailing dots, exact hosts remain explicit, and
  `*.` wildcard host patterns match child subdomains without also matching the
  bare base domain.
- Added a `v1.0.4` release sensitive-material gate that scans generated Linux
  tarballs and Windows zip artifacts for private keys, credential-like
  filenames, and common token patterns before publication.
- Added a `v1.0.3` request rewrite action for portable `200` URL rewrites,
  exposing bounded policy-core internal rewrite decisions through
  `ANIXOPS_REWRITE_URL_REWRITE_200` without claiming adapter forwarding
  behavior.
- Added a `v1.0.2` Loon metadata descriptor parser path that exposes
  `#!`/`[Plugin]` `name`, `desc`, `author`, `icon`, and `homepage` through a
  read-only C ABI without creating policy behavior or platform UI claims.
- Confirmed repository-level `main` branch protection, `v*` tag ruleset, and
  `github-release-publication` environment approval evidence required before
  `v1.0.0` publication.
- Fixed the tag-triggered release publish job to provide explicit GitHub
  repository context and document the `v1.0.0` tag-only failed publication
  attempt in replacement release notes.
- Started the v1.0.0 governance track with repository audit, roadmap,
  compatibility documentation skeleton, contribution rules, and manual
  intervention register.
- Added a pull request template that requires source contract, test,
  compatibility matrix, changelog, security, and manual-intervention evidence.
- Added a dedicated compatibility matrix GitHub Actions job that validates
  matrix row structure, status values, source contracts, and required evidence.
- Added a dedicated static format check GitHub Actions job for whitespace,
  CRLF, final newline, and shell syntax checks.
- Added a dedicated static lint GitHub Actions job using `shellcheck` for
  repository shell scripts.
- Added a macOS policy-core smoke GitHub Actions job for default C tests,
  runner checks, demo checks, and proxy-shim build checks.
- Added a release dry-run source contract before introducing release automation.
- Added a Loon common-fields source contract plus positive and negative parser
  fixtures for the first P1 parser milestone.
- Added a Loon plugin metadata source contract plus positive and negative
  parser fixtures for `[Plugin]` metadata diagnostics.
- Added a Loon hashbang metadata source contract plus positive and negative
  parser fixtures for tolerated `#!` metadata diagnostics.
- Added a Loon argument section source contract plus positive and negative
  parser fixtures for `[Argument]` defaults.
- Added a Loon inline-arguments source contract plus positive and negative
  parser fixtures for `#!arguments` script argument defaults.
- Added a Loon script metadata source contract plus positive and negative
  parser fixtures for request/response dispatch fields.
- Added a Loon task metadata source contract plus positive and negative parser
  fixtures for scheduled task descriptors.
- Added a Loon unsupported script-type guard proving `type=dns`, `type=rule`,
  and `type=policy` remain ignored until separately contracted.
- Added a Loon MITM certificate-material guard proving `ca-p12`,
  `ca-passphrase`, and `ca-cert` remain ignored and do not establish trust.
- Added Loon `[Header Rewrite]` header mutation parser fixtures and CI-covered
  tests while keeping platform header maps and HTTP serialization
  adapter-owned.
- Added portable/common response header add/replace parser fixtures and
  CI-covered tests while keeping platform header maps and HTTP serialization
  adapter-owned.
- Added portable/common request header regex parser fixtures and CI-covered
  tests while keeping platform header maps and HTTP serialization
  adapter-owned.
- Added Loon `[Body Rewrite]` body mutation parser fixtures and CI-covered
  tests while keeping streaming, compression, and HTTP serialization
  adapter-owned.
- Added Loon `[Body Rewrite]` request body JSON mutation parser fixtures and
  CI-covered tests while keeping streaming, compression, and HTTP serialization
  adapter-owned.
- Added Loon `[Body Rewrite]` response body JSON mutation parser fixtures and
  CI-covered tests while keeping streaming, compression, and HTTP serialization
  adapter-owned.
- Added portable/common response body JSON mutation parser fixtures and
  CI-covered tests while keeping streaming, compression, and HTTP serialization
  adapter-owned.
- Added portable/common request body JSON mutation parser fixtures and
  CI-covered tests while keeping streaming, compression, and HTTP serialization
  adapter-owned.
- Added portable/common request and response body JQ mutation parser fixtures
  and CI-covered fail-open tests while keeping production JQ runtime limits and
  HTTP serialization adapter-owned.
- Added portable/common HTTP JQ alias mutation parser fixtures and CI-covered
  fail-open tests while keeping production JQ runtime limits and HTTP
  serialization adapter-owned.
- Added portable/common request redirect status parser fixtures and CI-covered
  tests for the `301`/`302`/`303`/`307`/`308` request rewrite subset while
  keeping direct/proxy routing adapter-owned.
- Added portable/common echo-response parser fixtures and CI-covered tests
  while keeping HTTP serialization, streaming, and framing adapter-owned.
- Added an alpha fixture package check so release artifacts include all
  top-level compatibility fixtures instead of relying on a hand-maintained
  allowlist.
- Added a compatibility evidence check tying documented fixture paths and
  `config/...` parser-test IDs back to repository files and test registration.
- Extended the compatibility evidence check to cover fenced/plain fixture paths
  and documented script bundle manifest fixtures, plus orphan top-level fixture
  detection.
- Extended compatibility evidence checks to validate registered `config/...`,
  `script/...`, `rewrite/...`, `mitm/...`, and `abi/...` C test IDs, and
  hardened compatibility matrix rows so partial/supported positive and negative
  test evidence must be traceable.
- Extended compatibility evidence checks to validate documented Make targets,
  CI-covered script paths, and Go/Rust binding test names used as compatibility
  evidence.
- Extended compatibility evidence checks to validate documented
  `tests/test_*.c` file references used as compatibility evidence.
- Added a Quantumult X rewrite/MITM common-config source contract plus positive
  and negative parser fixtures for the second P1 parser milestone.
- Added Quantumult X `url echo-response` parser fixtures and CI-covered tests
  while keeping HTTP serialization, content-type writeback, and streaming
  adapter-owned.
- Added Quantumult X `url response-body-replace-regex` parser fixtures and
  CI-covered tests while keeping HTTP serialization, compression, and streaming
  adapter-owned.
- Added Quantumult X `url header-add` parser fixtures and CI-covered tests
  while keeping platform header map behavior and HTTP serialization
  adapter-owned.
- Added Quantumult X `url header-replace` parser fixtures and CI-covered tests
  while keeping platform header map behavior and HTTP serialization
  adapter-owned.
- Added Quantumult X `url response-header-add` parser fixtures and CI-covered
  tests while keeping platform header map behavior and HTTP serialization
  adapter-owned.
- Added Quantumult X `url response-header-replace` parser fixtures and
  CI-covered tests while keeping platform header map behavior and HTTP
  serialization adapter-owned.
- Added Quantumult X `url response-header-del` parser fixtures and CI-covered
  tests while keeping platform header map behavior and HTTP serialization
  adapter-owned.
- Added Quantumult X `url response-header-replace-regex` parser fixtures and
  CI-covered tests while keeping platform header map behavior and HTTP
  serialization adapter-owned.
- Added Quantumult X `url header-replace-regex` parser fixtures and CI-covered
  tests while keeping platform header map behavior and HTTP serialization
  adapter-owned.
- Added Quantumult X `url header-del` parser fixtures and CI-covered tests
  while keeping platform header map behavior and HTTP serialization
  adapter-owned.
- Added a Quantumult X MITM options source contract plus positive and negative
  parser fixtures for host/options adapter signals.
- Added a Quantumult X MITM certificate-material and validation-bypass guard
  proving `passphrase`, `p12`, and `skip_validating_cert` remain ignored and do
  not establish trust.
- Added a Quantumult X task metadata source contract plus positive and negative
  parser fixtures for `[task_local]` cron descriptors.
- Added Quantumult X `[task_local]` `event-network` and `event-interaction`
  parser descriptors while keeping event dispatch adapter-owned.
- Added a Quantumult X unsupported event trigger guard proving unknown event
  forms stay ignored and do not register task descriptors.
- Added a Surge module common-config source contract plus positive and negative
  parser fixtures for the third P1 parser milestone.
- Added Surge `[URL Rewrite]` `response-body-replace-regex` parser fixtures
  and CI-covered tests while keeping HTTP serialization, compression, and
  streaming adapter-owned.
- Added Surge `[URL Rewrite]` `response-body-json-replace` parser fixtures and
  CI-covered tests while keeping HTTP serialization, compression, and streaming
  adapter-owned.
- Added Surge `[URL Rewrite]` header mutation parser fixtures and CI-covered
  tests while keeping platform header maps and HTTP serialization adapter-owned.
- Added a Surge requirement metadata source contract plus positive and negative
  parser fixtures for `#!requirement` diagnostics.
- Added a Surge task metadata source contract plus positive and negative parser
  fixtures for `[Script]` cron and interval task descriptors.
- Added Surge `[Script]` `type=event` parser descriptors for
  `network-changed` and `notification` events while keeping event dispatch
  adapter-owned.
- Added a Surge event parser guard proving unknown event names are rejected
  instead of becoming unverified task descriptors.
- Added a Surge unsupported script-type guard proving `type=dns`, `type=rule`,
  and `type=policy-group` remain ignored until separately contracted.
- Added a Surge MITM certificate-material guard proving `ca-p12` and
  `ca-passphrase` remain ignored and do not establish trust.
- Added a Stash HTTP MITM source contract plus positive and negative parser
  fixtures for `http.mitm` host policy metadata.
- Added Stash HTTP MITM `host:*` port-wildcard fixture coverage, normalized to
  host-only policy-core matching while keeping port-specific behavior
  adapter-owned.
- Added a Stash HTTP MITM certificate-material guard proving `http.ca` and
  `http.ca-passphrase` remain ignored and do not establish trust.
- Added a Shadowrocket common-config source contract plus positive and negative
  parser fixtures for a P1 parser milestone.
- Added a Shadowrocket MITM certificate-material guard proving `ca-p12`,
  `ca-passphrase`, and `ca-cert` remain ignored and do not establish trust.
- Aligned v1 release checklist and README evidence links with GitHub
  Actions-only release acceptance and the v1 compatibility matrix path.
- Added a request rewrite source contract plus positive and negative parser
  fixtures for the first P2 rule-matching milestone.
- Added Loon `[URL Rewrite]` request redirect/reject parser fixtures and
  CI-covered tests while keeping direct/proxy route selection adapter-owned.
- Added Quantumult X `url` request redirect/reject parser fixtures and
  CI-covered tests while keeping direct/proxy route selection adapter-owned.
- Added Surge `[URL Rewrite]` request redirect/reject parser fixtures and
  CI-covered tests while keeping direct/proxy route selection adapter-owned.
- Added Shadowrocket `[URL Rewrite]` request redirect/reject parser fixtures
  and CI-covered tests while keeping direct/proxy route selection adapter-owned.
- Added a header mutation source contract plus positive and negative parser
  fixtures for the second P2 rule-matching milestone.
- Added Shadowrocket `[URL Rewrite]` header mutation parser fixtures and
  CI-covered tests while keeping platform header maps and HTTP serialization
  adapter-owned.
- Added a response rewrite source contract plus positive and negative parser
  fixtures for the third P2 rule-matching milestone.
- Added Loon `[URL Rewrite]` response echo/body-regex parser fixtures and
  CI-covered tests while keeping streaming, compression, and HTTP serialization
  adapter-owned.
- Added Quantumult X `url` response echo/body-regex parser fixtures and
  CI-covered tests while keeping streaming, compression, and HTTP serialization
  adapter-owned.
- Added Surge `[URL Rewrite]` response body-regex parser fixtures and
  CI-covered tests for the response-rewrite contract while keeping streaming,
  compression, and HTTP serialization adapter-owned.
- Added Shadowrocket `[URL Rewrite]` response mock body parser fixtures and
  CI-covered tests while keeping streaming, compression, and HTTP serialization
  adapter-owned.
- Aligned the Surge common source-contract evidence with the dedicated request
  rewrite parser fixtures and CI-covered tests.
- Added a body mutation source contract plus positive and negative parser
  fixtures for the fourth P2 rule-matching milestone.
- Added Shadowrocket `[URL Rewrite]` response body regex mutation parser
  fixtures and CI-covered tests while keeping streaming, compression, and HTTP
  serialization adapter-owned.
- Added a decision trace schema source contract plus CI-covered fixtures for
  structured MITM, rewrite, mutation, script, and policy-intent evidence.
- Added a reject/direct/proxy policy-intent source contract plus CI-covered
  fixtures proving the reject subset and unsupported direct/proxy route
  boundary.
- Added plan API parity fixtures and tests proving plan aggregation matches
  legacy URL/body/header/script evaluation for matched and mismatched phases.
- Added current-header plan parity checks and Go/Rust wrapper helpers for
  header regex rewrite planning.
- Added malformed MITM hostname parser and runtime negative tests so invalid
  hosts do not register or intercept through wildcard policy.
- Added a script runtime common source contract plus no-network replay evidence
  for double `$done` first-wins behavior.
- Added a cron/task trigger source contract and parser descriptor ABI while
  keeping scheduler/runtime support out of the supported surface.
- Added cron/task parser fixtures proving supported task descriptors, ignored
  unsupported scheduler forms, malformed cron rejection, and separation from
  HTTP script URL dispatch.
- Added a script runtime dependency decision recording no embedded QuickJS,
  JavaScriptCore, or other production JavaScript engine in the v1 policy core.
- Added a certificate lifecycle architecture contract, manual-intervention
  markers, and CI security-claim checks for root trust and target-host safety.
- Added an adapter redaction policy contract, manual-intervention marker, and
  CI security-claim checks for default raw payload/header logging claims.
- Added updated NetworkCore integration notes for the `v0.45.10-alpha`
  adapter baseline, deferred live-mutation gates, and v1.0.0 host-owned data
  plane boundary.
- Added a shared binding parity fixture with C runner, Go wrapper, and Rust
  wrapper CI coverage.
- Added named-header current-value parity checks for Go and Rust wrappers
  against the shared binding fixture.
- Added runner golden JSON trace fixtures for binding parity request and
  response traces.
- Added runner MITM decision golden JSON trace fixtures for TCP allow and QUIC
  fallback decisions.
- Added runner negative MITM decision golden JSON trace fixtures for
  certificate-not-trusted, deny-host, and no-host-match bypass decisions.
- Added runner disabled and malformed MITM decision golden JSON trace fixtures
  for disabled, empty-host, and invalid-host bypass decisions.
- Added Stash and Shadowrocket migration notes that keep app-profile behavior
  planned outside dedicated parser fixtures and tests.
- Added Stash and Shadowrocket migration guard fixtures proving unsupported
  app-level profile syntax remains ignored until dedicated parser support
  exists.
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
- Added a manual-intervention register schema check and wired it into the full
  GitHub Actions gate.
- Hardened the manual-intervention schema so pending required markers must
  carry exactly one `confirmation-evidence=not-yet-confirmed` placeholder.
- Hardened the manual-intervention schema so required markers must have unique
  status, scope, release-boundary, evidence, and pending next-action fields.
- Hardened the manual-intervention schema so completed markers must carry
  explicit non-placeholder confirmation evidence.
- Added Stash `http.force-http-engine` parser fixtures and CI-covered tests as
  an adapter-visible QUIC fallback signal without claiming HTTP engine runtime
  behavior.
- Added Stash `http.url-rewrite` reject parser fixtures and CI-covered tests
  while keeping transparent rewrite and direct/proxy route selection
  adapter-owned.
- Added Stash `http.url-rewrite` 302/307 redirect parser fixtures and
  CI-covered tests while keeping transparent rewrite and route selection
  adapter-owned.
- Added Stash `http.url-rewrite` 301/303/308 redirect parser fixtures and
  CI-covered tests, aligned with the portable redirect status subset.
- Added Shadowrocket `[Rule]` URL-regex reject parser fixtures and CI-covered
  tests while keeping direct/proxy route selection adapter-owned.
- Added Shadowrocket `[Rule]` DOMAIN reject parser fixtures and CI-covered
  tests while keeping direct/proxy route selection adapter-owned.
- Added Shadowrocket `[Rule]` DOMAIN-KEYWORD reject parser fixtures and
  CI-covered tests while keeping direct/proxy route selection adapter-owned.
- Added Shadowrocket `[Rule]` DOMAIN-SUFFIX reject parser fixtures and
  CI-covered tests while keeping direct/proxy route selection adapter-owned.
- Added Shadowrocket `[Rule]` FINAL reject parser fixtures and CI-covered tests
  while keeping direct/proxy route selection adapter-owned.
- Added Loon `[MITM]` option parser fixtures and CI-covered tests for
  adapter-visible `enable`, `hostname`, `skip-server-cert-verify`, `h2`, and
  `disable-quic` signals without claiming certificate lifecycle or TLS support.
- Added Loon `[Rule]` DOMAIN-SUFFIX reject parser fixtures and CI-covered tests
  while keeping direct/proxy route selection adapter-owned.
- Added Loon `[Rule]` DOMAIN reject parser fixtures and CI-covered tests while
  keeping direct/proxy route selection adapter-owned.
- Added Loon `[Rule]` DOMAIN-KEYWORD reject parser fixtures and CI-covered
  tests while keeping direct/proxy route selection adapter-owned.
- Added Loon `[Rule]` FINAL reject parser fixtures and CI-covered tests while
  keeping direct/proxy route selection adapter-owned.
- Added Loon `[Rule]` URL-REGEX reject parser fixtures and CI-covered tests
  while keeping direct/proxy route selection adapter-owned.
- Added a Loon `[Rule]` route-selection guard fixture and CI-covered test
  proving direct/proxy, `IP-CIDR`, `GEOIP`, and `no-resolve` remain
  adapter-owned.
- Added compatibility-matrix validation requiring non-support guard rows to use
  `unsupported` status rather than `planned`.
- Added a release-readiness gate that blocks stable `v1.0.0` publication while
  required manual-intervention markers remain pending.
- Added compatibility matrix status counts to release and dry-run manifests,
  notes, and GitHub Step Summaries.
- Added Windows x64 release artifact generation, checksum validation, and
  manifest coverage to the tag-triggered release workflow.
- Added Windows x64 dry-run artifact generation, checksum validation, manifest
  coverage, release notes, and Step Summary evidence to keep release dry-run
  metadata aligned with the tag-triggered release artifact set.
- Added a v1.0.0 acceptance evidence check that ties release readiness,
  compatibility status, manual-intervention markers, CI jobs, and release-gate
  metadata into the GitHub Actions governance baseline.
- Added a repository governance contract and static check for branch
  protection, protected `v*` release tags, and `github-release-publication`
  environment approval evidence.
- Added a release metadata static check for release and dry-run artifact,
  checksum, manifest, release-note, compatibility-count, and
  publication-boundary evidence.
- Added a CI trigger static check for PR, main push, tag, manual dry-run, and
  tag-release workflow coverage.
- Added a release checklist static check covering v1 stable release workflow,
  artifact, manifest, checksum, release-note, and manual gate evidence.
- Hardened manual-intervention checks so required markers can transition from
  pending to confirmed only with non-placeholder confirmation evidence.
- Added a manual-intervention transition fixture check covering confirmed
  marker evidence validation.
- Added direct manual-intervention evidence and transition gates to release and
  release dry-run workflows before stable release-readiness evaluation.
- Added direct release checklist and metadata static gates to release and
  release dry-run workflows before stable release-readiness evaluation.
- Hardened release metadata checks so compatibility summary count keys must be
  present exactly once in release and release dry-run workflows.
- Added a compatibility status summary static check requiring release count
  outputs to be unique, non-negative integers, and consistent with the total
  compatibility matrix row count.

### Changed

- Clarified that GitHub Actions is the acceptance source of truth for build,
  test, package, release dry-run, and release publication.
- Updated the ROADMAP current-track marker and repository audit test-status
  language so governance docs no longer imply the project is still at P0 only
  or rely on hard-coded local test counts.

## v0.45.10-alpha - 2026-07-08

### Added

- Published Alpha artifacts for Linux and Windows.
- Added runner and proxy-shim coverage for script exception fail-open behavior.
- Included script-runtime error fixture in the Alpha package.

### Notes

- This Alpha proves a policy-core and no-UI runner foundation. It is not a
  v1.0.0 compatibility claim.
